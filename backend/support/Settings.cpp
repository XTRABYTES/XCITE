/**
 * Filename: Settings.cpp
 *
 * XCITE is a secure platform utilizing the XTRABYTES Proof of Signature
 * blockchain protocol to host decentralized applications
 *
 * Copyright (c) 2017-2018 Zoltan Szabo & XTRABYTES developers
 *
 * This file is part of an XTRABYTES Ltd. project.
 *
 */

#include "Settings.hpp"
#include <openssl/pem.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <cstdint>
#include <cstring>
#include <tuple>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include "DownloadManager.hpp"
#include "../xchat/xchat.hpp"

#ifdef Q_OS_ANDROID //added to get write permission for Android
#include <QtAndroid>
#endif

using std::string;
using std::cin;

Settings::Settings(QObject *parent) :
    QObject(parent)
{

}

Settings::Settings(QQmlApplicationEngine *engine, QSettings *settings, QObject *parent) :
    QObject(parent)
{
    m_engine = engine;
    m_settings = settings;

}

void Settings::onCheckOS() {
    emit oSReturned(QSysInfo::productType());
}

void Settings::createRand(int limit) {
    int n = QRandomGenerator::global()->bounded(limit);
    emit randReturn(n);
}

void Settings::setLocale(QString locale) {
    if (!m_translator.isEmpty()) {
        QCoreApplication::removeTranslator(&m_translator);
    }

    if (locale != "en_us") {
        QString localeFile = QStringLiteral(":/i18n/lang_") + locale;
        if (!m_translator.load(localeFile)) {
            return;
        }

        QCoreApplication::installTranslator(&m_translator);
    }

    m_engine->retranslate();
}

void Settings::onLocaleChange(QString locale) {
    setLocale(locale);
}

void Settings::onClearAllSettings() {
    bool fallbacks = m_settings->fallbacksEnabled();
    m_settings->setFallbacksEnabled(false);
    m_settings->remove("accountCreationCompleted");
    m_settings->remove("developer");
    m_settings->remove("xchat");
    m_settings->remove("width");
    m_settings->remove("height");
    m_settings->remove("x");
    m_settings->remove("y");
    m_settings->remove("onboardingCompleted");
    m_settings->remove("defaultCurrency");
    m_settings->remove("locale");
    m_settings->remove("pinlock");
    m_settings->remove("theme");
    m_settings->remove("xby");
    m_settings->remove("xfuel");
    m_settings->remove("xbytest");
    m_settings->remove("xfueltest");
    m_settings->remove("tagMe");
    m_settings->remove("tagEveryone");
    m_settings->remove("xChatDND");
    m_settings->remove("showBalance");
    m_settings->sync();

    m_settings->setFallbacksEnabled(fallbacks);

    emit clearSettings();
}

// Onboarding and login functions

bool Settings::UserExists(QString username){
    if(checkInternet("http://37.59.57.212:8080")){
        bool userExists = false;
        QTimer timeout;

        QEventLoop loop;
        timeout.setSingleShot(true);
        timeout.start(6000);
        connect(&timeout, SIGNAL(timeout()), &loop, SLOT(quit()),Qt::QueuedConnection);
        auto connectionHandler =  connect(this, &Settings::userExistsSignal, [&userExists,&loop](bool checked) {
            userExists = checked;
            loop.quit();

        });

        QString url = "http://37.59.57.212:8080/v1/user/" + username;
        URLObject urlObj {QUrl(url)};
        urlObj.addProperty("route","userExistsSlot");
        DownloadManagerHandler(&urlObj);

        loop.exec();
        disconnect(connectionHandler);
        timeout.deleteLater();
        return userExists;
    }
    else {
        qDebug() << "no connection to account server to check if user exists";
        emit noInternet();
        return false;
    }
}

QString Settings::RestAPIPostCall(QString apiURL, QByteArray payloadToSend){
    QString statusCode = "";
    QUrl Url;
    Url.setScheme("http");
    Url.setHost("37.59.57.212");
    Url.setPort(8080);
    Url.setPath(apiURL);
    bool success = false;
    QString payload = "";
    QObject context;
    QTimer timeout;
    QEventLoop loop;
    timeout.setSingleShot(true);
    timeout.start(6000);
    connect(&timeout, SIGNAL(timeout()), &loop, SLOT(quit()),Qt::QueuedConnection);
    auto connectionHandler = connect(this, &Settings::apiPostSignal, &context,[&payload, &success, &loop](bool status,QByteArray payloadIncoming) {
        payload = QString(payloadIncoming);
        success = status;
        loop.quit();
    });
    URLObject urlObj {Url};
    urlObj.addProperty("route","apiPostSlot");
    urlObj.addProperty("POST",true);
    urlObj.addProperty("payload",payloadToSend);
    DownloadManagerHandler(&urlObj);

    loop.exec();

    disconnect(connectionHandler);
    timeout.deleteLater();


    return payload;

}


void Settings::CreateUser(QString username, QString password){
    if (checkInternet("http://37.59.57.212:8080")) {
        QTextCodec::setCodecForLocale(QTextCodec::codecForName("Latin1"));
        QAESEncryption encryption(QAESEncryption::AES_128, QAESEncryption::ECB);

        emit checkUsername();
        if(UserExists(username)){
            qDebug() << "User exists pre create check";

            return;
        }else{

            QVariantMap settings;
            settings.insert("app","xtrabytes");
            m_settings->setValue("app","xtrabytes");

            emit createUniqueKeyPair();
            // Create Pub/Priv RSA Key
            keyPair = createKeyPair();
            QByteArray pubKey = keyPair.first;
            QByteArray privKey = keyPair.second;

            int padding = RSA_PKCS1_OAEP_PADDING;
            unsigned char decrypted[32];

            QString pubKeyString = QString::fromLatin1(pubKey,pubKey.size());
            QVariantMap feed1;
            feed1.insert("pubKey", pubKeyString);
            feed1.insert("username",username);

            QByteArray payload =  QJsonDocument::fromVariant(feed1).toJson(QJsonDocument::Compact);
            payload = payload.toBase64();

            //  Send Pub Key to API.  Response contains backend AES key + iv
            QString response2 = RestAPIPostCall("/v1/createKeyPair", payload);
            if (response2.isEmpty()){
                return;
            }

            QJsonDocument jsonResponse = QJsonDocument::fromJson(response2.toLatin1());
            QJsonValue encryptedText = jsonResponse.object().value("aeskey");
            QByteArray aeskeyEncrypted = encryptedText.toString().toLatin1();

            emit receiveSessionEncryptionKey();
            // Get IV from API to use in future encryptions
            QJsonValue ivValue = jsonResponse.object().value("iv");
            QByteArray iv = ivValue.toString().toLatin1();

            const std::size_t aesKeySize = aeskeyEncrypted.size();
            unsigned char* encrypted = new unsigned char[aesKeySize];
            std::memcpy(encrypted,aeskeyEncrypted.constData(),aesKeySize);
            unsigned char* privKey2 = new unsigned char[privKey.size()];
            std::memcpy(privKey2,privKey.data(),privKey.size());
            RSA * privRSAKey = createRSA(privKey2,0);


            // Decrypt AES key using local private key. Stores it to backendKey
            int  decryptedSize = RSA_private_decrypt(aesKeySize,encrypted,backendKey,privRSAKey,padding);
            qDebug() << decryptedSize;

            emit saveAccountSettings();
            // Save iv data to local storage
            std::memcpy(iiiv,iv.constData(),iv.size());

            /* Message to be encrypted */
            QString randNum = createRandNum();

            // Encrypt randNumber with password
            QByteArray encodedRandNr = encryption.encode(randNum.toLatin1(), (password + "xtrabytesxtrabytes").toLatin1());
            QString encodedRandNrStr = QString::fromLatin1(encodedRandNr, encodedRandNr.length());

            // Encrypt randNum with backend AES key
            std::pair<int, QByteArray> cipher = encryptAes(randNum, backendKey, iiiv);

            cipher.second = cipher.second.toBase64();

            QByteArray settingsByte =  QJsonDocument::fromVariant(settings).toJson(QJsonDocument::Compact);

            QByteArray encodedText = encryption.encode(settingsByte, (password + "xtrabytesxtrabytes").toLatin1());
            QString DataAsString = QString::fromLatin1(encodedText, encodedText.length());

            QVariantMap feed2;
            feed2.insert("encrypted", cipher.second);
            feed2.insert("randNumPass", encodedRandNrStr);
            feed2.insert("randNum", randNum);
            feed2.insert("username", username);
            feed2.insert("settings", DataAsString);


            QByteArray payload3 =  QJsonDocument::fromVariant(feed2).toJson(QJsonDocument::Compact);
            payload3 = payload3.toBase64();

            // Send encrypted rand number + settings.  Backend checks randNum and saves settings.  Returns encrypted sessionId
            QString response3 = RestAPIPostCall("/v1/decryptAES", payload3);
            if (response3.isEmpty()){
                return;
            }

            emit receiveSessionID();
            QJsonDocument jsonResponse2 = QJsonDocument::fromJson(response3.toLatin1());
            QJsonValue encryptedText2 = jsonResponse2.object().value("sessionId");
            QByteArray sessionIdEncrypted = encryptedText2.toString().toLatin1();

            const std::size_t sessionIdSize = sessionIdEncrypted.size();
            unsigned char* encryptedSess = new unsigned char[sessionIdSize];
            std::memcpy(encryptedSess,sessionIdEncrypted.constData(),sessionIdSize);

            // Decrypt session Id using local RSA keys
            int  decryptedSize2 = RSA_private_decrypt(sessionIdSize,encryptedSess,decrypted,privRSAKey,padding);

            QByteArray sessionIdBa;
            sessionIdBa = QByteArray(reinterpret_cast<char*>(decrypted), decryptedSize2);
            sessionId = QString::fromLatin1(sessionIdBa, sessionIdBa.size());


            if (UserExists(username)){
                qDebug() << "User exists post create check";

                m_username = username;
                m_password = password;
                emit userCreationSucceeded();

            }else{
                emit userCreationFailed();
            }

            delete [] encrypted;
            delete [] privKey2;
        }

    }
    else {
        qDebug() << "no connection to account server to create new user";
        emit noInternet();
        return;
    }
}

QString Settings::createRandNum(){
    srand (time (0));
    int a = 1000000000;
    int b = 10000000000;

    int num = (double)rand() / (RAND_MAX + 1) * (b - a) + a;
    QString randNum = QString::number(abs(num));
    randNum = randNum.chopped(randNum.length() - 9); //ensure value is 9 characters

    return randNum;
}

std::pair<QByteArray,QByteArray> Settings::createKeyPair(){

    const int kBits = 4096;
    const int kExp = 3;
    char *pem_key, *pem_key_pub;
    BIGNUM *e;
    e=BN_new();
    BN_set_word(e, 17);
    RSA *rsa_keypair = RSA_new();
    RSA_generate_key_ex(rsa_keypair, 1024, e, NULL);

    //Private key in PEM form:
    BIO *bio = BIO_new(BIO_s_mem());
    PEM_write_bio_RSAPrivateKey(bio, rsa_keypair, NULL, NULL, 0, NULL, NULL);
    int keylen = BIO_pending(bio);
    pem_key = (char *)malloc(keylen); /* Null-terminate */
    BIO_read(bio, pem_key, keylen);
    QByteArray privkey = QByteArray::fromRawData(pem_key, keylen);

    //Public key in PEM form:
    BIO *bio2 = BIO_new(BIO_s_mem());
    PEM_write_bio_RSA_PUBKEY(bio2, rsa_keypair);
    int keylen2 = BIO_pending(bio2);
    pem_key_pub = (char *)malloc(keylen2); /* Null-terminate */
    BIO_read(bio2, pem_key_pub, keylen2);
    QByteArray pubkey = QByteArray::fromRawData(pem_key_pub, keylen2);
    std::pair<QByteArray,QByteArray> returnVal(pubkey,privkey);

    return returnVal;
}


RSA * Settings::createRSA(unsigned char * key,int public1)
{
    RSA *rsa= NULL;
    BIO *keybio ;
    keybio = BIO_new_mem_buf(key, -1);
    if (keybio==NULL)
    {
        printf( "Failed to create key BIO");
        return 0;
    }
    if(public1)
    {
        rsa = PEM_read_bio_RSA_PUBKEY(keybio, &rsa,NULL, NULL);
    }
    else
    {
        rsa = PEM_read_bio_RSAPrivateKey(keybio, &rsa,NULL, NULL);
    }
    if(rsa == NULL)
    {
        printf( "Failed to create RSA");
    }

    return rsa;
}

void Settings::login(QString username, QString password){
    loginFile(username,password,"default");
}

void Settings::ImportWallet(QString username, QString password){
    loginFile(username,password,"import");
}

void Settings::RestoreAccount(QString username, QString password) {
    loginFile(username,password,"restore");
}

void Settings::loginFile(QString username, QString password, QString fileLocation){
    if (checkInternet("http://37.59.57.212:8080")) {
        emit checkUsername();
        if(!UserExists(username)){
            qDebug() << "User does not exist";
            return;
        }
        QAESEncryption encryption(QAESEncryption::AES_128, QAESEncryption::ECB);

        // Create Pub/Priv RSA Key
        emit createUniqueKeyPair();
        keyPair = createKeyPair();
        QByteArray pubKey = keyPair.first;
        QByteArray privKey = keyPair.second;

        int padding = RSA_PKCS1_OAEP_PADDING;
        unsigned char decrypted[32];

        QString pubKeyString = QString::fromLatin1(pubKey,pubKey.size());
        QVariantMap feed1;
        feed1.insert("pubKey", pubKeyString);
        feed1.insert("username",username);

        QByteArray payload =  QJsonDocument::fromVariant(feed1).toJson(QJsonDocument::Compact);
        payload = payload.toBase64();

        //  Send Pub Key to API.  Backend returns AES key and randNum encrypted with password

        QString response2 = RestAPIPostCall("/v1/login", payload);
        if (response2.isEmpty()){
            return;
        }

        QJsonDocument jsonResponse = QJsonDocument::fromJson(response2.toLatin1());
        QJsonValue encryptedText = jsonResponse.object().value("aeskey");
        QByteArray aeskeyEncrypted = encryptedText.toString().toLatin1();

        // Get IV from API to use in future encryptions
        QJsonValue ivValue = jsonResponse.object().value("iv");
        QByteArray iv = ivValue.toString().toLatin1();

        // Check encrypted randNr
        emit checkIdentity();
        QJsonValue randNumEnc = jsonResponse.object().value("randNum");
        QByteArray randNumBa = randNumEnc.toString().toLatin1();

        unsigned char* privKey2 = new unsigned char[privKey.size()];
        std::memcpy(privKey2,privKey.data(),privKey.size());

        RSA * privRSAKey = createRSA(privKey2,0);

        delete [] privKey2;
        const std::size_t randNumBaSize = randNumBa.size();
        unsigned char* encryptedRandNum = new unsigned char[randNumBaSize];
        std::memcpy(encryptedRandNum,randNumBa.constData(),randNumBaSize);

        // Decrypt rand Number sent from DB
        int  decryptedSize = RSA_private_decrypt(randNumBaSize,encryptedRandNum,decrypted,privRSAKey,padding);

        QByteArray randNumDec1;
        randNumDec1 = QByteArray(reinterpret_cast<char*>(decrypted), decryptedSize);

        //Decrypt decrypted randNum with password
        QByteArray decodedRandNum = encryption.decode(randNumDec1, (password + "xtrabytesxtrabytes").toLatin1());
        QString randNumDec = QString::fromLatin1(decodedRandNum);
        randNumDec = randNumDec.chopped(randNumDec.length() - 9); //ensure value is 9 characters

        emit receiveSessionEncryptionKey();
        //Decrypt the AES key with local private key
        const std::size_t aesKeySize = aeskeyEncrypted.size();
        unsigned char* aeskeyEncryptedChar = new unsigned char[aesKeySize];
        std::memcpy(aeskeyEncryptedChar,aeskeyEncrypted.constData(),aesKeySize);
        int  decryptedSize1 = RSA_private_decrypt(aesKeySize,aeskeyEncryptedChar,backendKey,privRSAKey,padding);
        qDebug() << decryptedSize;

        std::memcpy(iiiv,iv.constData(),iv.size());

        // encrypt decrypted randNum with backendKey
        std::pair<int, QByteArray> cipher = encryptAes(randNumDec, backendKey, iiiv);
        cipher.second = cipher.second.toBase64();

        QVariantMap feed2;
        feed2.insert("randNumDec", cipher.second);
        feed2.insert("username",username);
        feed2.insert("iv",iv);

        QByteArray payload2 =  QJsonDocument::fromVariant(feed2).toJson(QJsonDocument::Compact);
        payload2 = payload2.toBase64();


        //  Send Decrypted randNum back to backend (checks if user is right user)
        QString response3 = RestAPIPostCall("/v1/checkUser", payload2);
        if (response3.isEmpty()){
            emit loginFailedChanged();
            return;
        }
        QJsonDocument jsonResponse2 = QJsonDocument::fromJson(response3.toLatin1());
        QJsonValue encryptedSettings = jsonResponse2.object().value("settings");
        QString settings = encryptedSettings.toString();

        QJsonValue encryptedSessionId = jsonResponse2.object().value("sessionId");
        QByteArray sessionIdEncrypted = encryptedSessionId.toString().toLatin1();

        const std::size_t sessionIdSize = sessionIdEncrypted.size();
        unsigned char* encryptedSess = new unsigned char[sessionIdSize];
        std::memcpy(encryptedSess,sessionIdEncrypted.constData(),sessionIdSize);

        // Decrypt sessionId
        emit receiveSessionID();
        int  decryptedSize2 = RSA_private_decrypt(sessionIdSize,encryptedSess,decrypted,privRSAKey,padding);

        QByteArray sessionIdBa = QByteArray(reinterpret_cast<char*>(decrypted), decryptedSize2);
        sessionId = QString::fromLatin1(sessionIdBa, sessionIdBa.size());

        QByteArray decodedSettings = encryption.decode(settings.toLatin1(), (password + "xtrabytesxtrabytes").toLatin1());
        int pos = decodedSettings.lastIndexOf(QChar('}')); // find last bracket to mark the end of the json
        decodedSettings = decodedSettings.left(pos+1); //remove everything after the valid json
        QJsonObject decodedJson = QJsonDocument::fromJson(decodedSettings).object();

        if (fileLocation == "restore") {
            QString settingsFile = LoadFile(username.toLower() + ".backup", fileLocation);
            if (settingsFile != "ERROR"){
                decodedSettings = encryption.decode(settingsFile.toLatin1(), (password + "xtrabytesxtrabytes").toLatin1());
                int pos = decodedSettings.lastIndexOf(QChar('}')); // find last bracket to mark the end of the json
                int begin = decodedSettings.indexOf(QChar('{')); // find first bracket to mark beginning of the json
                qDebug() << "first { at poistion: " << begin;
                decodedSettings = decodedSettings.left(pos+1); // remove everything after the valid json
                decodedJson = QJsonDocument::fromJson(decodedSettings).object();
            }else{
                emit loginFailedChanged();
                return;
            }
        }
        else {
            // do nothing
        }

        bool checkPermission = true;
#ifdef Q_OS_ANDROID //added to get write permission for Android
        auto  result = QtAndroid::checkPermission(QString("android.permission.WRITE_EXTERNAL_STORAGE"));
        if(result == QtAndroid::PermissionResult::Denied){
            QtAndroid::PermissionResultMap resultHash = QtAndroid::requestPermissionsSync(QStringList({"android.permission.WRITE_EXTERNAL_STORAGE"}));
            if(resultHash["android.permission.WRITE_EXTERNAL_STORAGE"] == QtAndroid::PermissionResult::Denied){
                checkPermission = false;
            }
        }
#endif

        if(decodedJson.value("app").toString().startsWith("xtrabytes")){
            m_username = username;
            m_password = password;
            if (checkPermission) {
                QString currentDir = QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation)[0] + "/XCITE_backup";
                QString fileName = username.toLower() + ".backup";
                QDir dir;
                dir.mkpath(currentDir);
                QString fullFileName = currentDir + "/" + fileName;
                bool exists = QFile::exists(fullFileName);
                if (!exists) {
                    xchatRobot.SubmitMsg("backend - creating backupfile: " + fullFileName);
                    QFile file(currentDir + "/" + fileName);
                }
                else {
                    xchatRobot.SubmitMsg("backend - backupfile exists: " + fullFileName);
                }
            }

            emit loadingSettings();
            QString location;
            if (fileLocation == "restore") {
                location = "default";
            }
            else {
                location = fileLocation;
            }
            LoadSettings(decodedSettings, location);

            // Create new rand Num.
            QString randNum = createRandNum();

            QByteArray encodedRandNr = encryption.encode(randNum.toLatin1(), (password + "xtrabytesxtrabytes").toLatin1());
            QString encodedRandNrStr = QString::fromLatin1(encodedRandNr, encodedRandNr.length());

            std::pair<int, QByteArray> randNumAes = encryptAes(randNum, backendKey, iiiv);

            randNumAes.second = randNumAes.second.toBase64();

            std::pair<int, QByteArray> sessionIdAes = encryptAes(sessionId, backendKey, iiiv);

            sessionIdAes.second = sessionIdAes.second.toBase64();

            QVariantMap feed3;
            feed3.insert("randNumAes", randNumAes.second);
            feed3.insert("randNumPass", encodedRandNrStr);
            feed3.insert("sessionIdAes", sessionIdAes.second);
            feed3.insert("username",username);

            QByteArray finalLogin =  QJsonDocument::fromVariant(feed3).toJson(QJsonDocument::Compact);

            finalLogin = finalLogin.toBase64();

            // Send new encrypted Rand Nums to backend
            QString finalLoginResponse = RestAPIPostCall("/v1/finalLogin", finalLogin);

            if (finalLoginResponse.isEmpty()){
                emit loginFailedChanged();
                return;
            }

            broker.me = m_username;
            broker.connectExchange("xchats");
            broker.connectExchange("xgames");

        }else{
            emit loginFailedChanged();
            return;
        }
    }
    else {
        qDebug() << "no connection to account server to log in";
        emit noInternet();
        return;
    }
}


void Settings::changePassword(QString oldPassword, QString newPassword){
    if (checkInternet("http://37.59.57.212:8080")) {
        QAESEncryption encryption(QAESEncryption::AES_128, QAESEncryption::ECB);
        QByteArray pubKey = keyPair.first;
        QByteArray privKey = keyPair.second;

        int padding = RSA_PKCS1_OAEP_PADDING;
        unsigned char decrypted[32];

        QString pubKeyString = QString::fromLatin1(pubKey,pubKey.size());
        QVariantMap feed1;
        feed1.insert("pubKey", pubKeyString);
        feed1.insert("username",m_username);

        QByteArray payload =  QJsonDocument::fromVariant(feed1).toJson(QJsonDocument::Compact);
        payload = payload.toBase64();

        //  Send Pub Key to API.  Backend returns AES key and randNum encrypted with password
        QString response2 = RestAPIPostCall("/v1/login", payload);
        if (response2.isEmpty()){
            return;
        }

        QJsonDocument jsonResponse = QJsonDocument::fromJson(response2.toLatin1());
        QJsonValue encryptedText = jsonResponse.object().value("aeskey");
        QByteArray aeskeyEncrypted = encryptedText.toString().toLatin1();

        // Get IV from API to use in future encryptions
        QJsonValue ivValue = jsonResponse.object().value("iv");
        QByteArray iv = ivValue.toString().toLatin1();

        // Check encrypted randNr
        QJsonValue randNumEnc = jsonResponse.object().value("randNum");
        QByteArray randNumBa = randNumEnc.toString().toLatin1();

        unsigned char* privKey2 = new unsigned char[privKey.size()];
        std::memcpy(privKey2,privKey.data(),privKey.size());

        RSA * privRSAKey = createRSA(privKey2,0);

        delete [] privKey2;
        const std::size_t randNumBaSize = randNumBa.size();
        unsigned char* encryptedRandNum = new unsigned char[randNumBaSize];
        std::memcpy(encryptedRandNum,randNumBa.constData(),randNumBaSize);

        // Decrypt rand Number sent from DB
        int  decryptedSize = RSA_private_decrypt(randNumBaSize,encryptedRandNum,decrypted,privRSAKey,padding);

        QByteArray randNumDec1;
        randNumDec1 = QByteArray(reinterpret_cast<char*>(decrypted), decryptedSize);

        //Decrypt decrypted randNum with password
        QByteArray decodedRandNum = encryption.decode(randNumDec1, (oldPassword + "xtrabytesxtrabytes").toLatin1());
        QString randNumDec = QString::fromLatin1(decodedRandNum);
        randNumDec = randNumDec.chopped(randNumDec.length() - 9); //ensure value is 9 characters

        //Decrypt the AES key with local private key
        const std::size_t aesKeySize = aeskeyEncrypted.size();
        unsigned char* aeskeyEncryptedChar = new unsigned char[aesKeySize];
        std::memcpy(aeskeyEncryptedChar,aeskeyEncrypted.constData(),aesKeySize);
        int  decryptedSize1 = RSA_private_decrypt(aesKeySize,aeskeyEncryptedChar,backendKey,privRSAKey,padding);
        qDebug() << decryptedSize;

        std::memcpy(iiiv,iv.constData(),iv.size());

        // encrypt decrypted randNum with backendKey
        std::pair<int, QByteArray> cipher = encryptAes(randNumDec, backendKey, iiiv);
        cipher.second = cipher.second.toBase64();

        QVariantMap feed2;
        feed2.insert("randNumDec", cipher.second);
        feed2.insert("username",m_username);
        feed2.insert("iv",iv);

        QByteArray payload2 =  QJsonDocument::fromVariant(feed2).toJson(QJsonDocument::Compact);
        payload2 = payload2.toBase64();

        //  Send Decrypted randNum back to backend (checks if user is right user)
        QString response3 = RestAPIPostCall("/v1/checkUser", payload2);
        if (response3.isEmpty()){
            emit passwordChangedFailed();

            return;
        }
        QJsonDocument jsonResponse2 = QJsonDocument::fromJson(response3.toLatin1());
        QJsonValue encryptedSettings = jsonResponse2.object().value("settings");
        QString settings = encryptedSettings.toString();

        QJsonValue encryptedSessionId = jsonResponse2.object().value("sessionId");
        QByteArray sessionIdEncrypted = encryptedSessionId.toString().toLatin1();

        const std::size_t sessionIdSize = sessionIdEncrypted.size();
        unsigned char* encryptedSess = new unsigned char[sessionIdSize];
        std::memcpy(encryptedSess,sessionIdEncrypted.constData(),sessionIdSize);

        // Decrypt sessionId
        int  decryptedSize2 = RSA_private_decrypt(sessionIdSize,encryptedSess,decrypted,privRSAKey,padding);

        QByteArray sessionIdBa = QByteArray(reinterpret_cast<char*>(decrypted), decryptedSize2);
        sessionId = QString::fromLatin1(sessionIdBa, sessionIdBa.size());

        QByteArray decodedSettings = encryption.decode(settings.toLatin1(), (oldPassword + "xtrabytesxtrabytes").toLatin1());
        int pos = decodedSettings.lastIndexOf(QChar('}')); // find last bracket to mark the end of the json
        decodedSettings = decodedSettings.left(pos+1); //remove everything after the valid json
        int begin = decodedSettings.indexOf(QChar('{')); // find first bracket to mark beginning of the json
        qDebug() << "first { at poistion: " << begin;
        QJsonObject decodedJson = QJsonDocument::fromJson(decodedSettings).object();

        if(decodedJson.value("app").toString().startsWith("xtrabytes")){
            // Create new rand Num.
            QString randNum = createRandNum();

            QByteArray encodedRandNr = encryption.encode(randNum.toLatin1(), (newPassword + "xtrabytesxtrabytes").toLatin1());
            QString encodedRandNrStr = QString::fromLatin1(encodedRandNr, encodedRandNr.length());

            std::pair<int, QByteArray> randNumAes = encryptAes(randNum, backendKey, iiiv);

            randNumAes.second = randNumAes.second.toBase64();

            std::pair<int, QByteArray> sessionIdAes = encryptAes(sessionId, backendKey, iiiv);

            sessionIdAes.second = sessionIdAes.second.toBase64();

            // create settings file
            m_password = newPassword;

            QVariantMap settings;
            foreach (const QString &key, m_settings->childKeys()) {//iterate through m_settings to add everything to settings file we write to DB
                settings.insert(key,m_settings->value(key).toString());
            }
            QString dec_pincode = encryption.decode(m_pincode.toLatin1(), (oldPassword + "xtrabytesxtrabytes").toLatin1());
            settings.insert("pincode", dec_pincode); //may be able to remove this

            /*      Add contacts to DB       */
            QJsonArray contactsArray = QJsonDocument::fromJson(m_contacts.toLatin1()).array();
            settings.insert("contacts",contactsArray.toVariantList()); // add contacts array to our existing settings
            qDebug().noquote() << contactsArray;


            /*      Add addresses to DB       */
            QJsonArray addressesArray = QJsonDocument::fromJson(m_addresses.toLatin1()).array(); //save addresses saves array to m_addresses
            settings.insert("addresses",addressesArray.toVariantList()); // add address array to our existing settings

            /*      Add pendingTransactions to DB       */
            QJsonArray pendingArray = QJsonDocument::fromJson(m_pending.toLatin1()).array();
            settings.insert("pendingList",pendingArray.toVariantList()); // add pendingTransactions array to our existing settings
            qDebug().noquote() << pendingArray;

            /*      Add wallets to DB       */
            bool localKeys = m_settings->value("localKeys").toBool();
            if (!localKeys){
                QJsonArray walletArray = QJsonDocument::fromJson(m_wallet.toLatin1()).array(); //savewallet saves array to m_wallet
                settings.insert("walletList",walletArray.toVariantList()); // add wallet array to our existing settings
            }

            /*    Convert Settings Variant to QByteArray  and encode it   */
            QByteArray settingsOutput =  QJsonDocument::fromVariant(QVariant(settings)).toJson(QJsonDocument::Compact); //Convert settings to byteArray/Json
            QByteArray encodedText = encryption.encode(settingsOutput, (m_password + "xtrabytesxtrabytes").toLatin1()); //encode settings after adding address
            QString DataAsString = QString::fromLatin1(encodedText, encodedText.length());

            /*      check encryption    */
            QByteArray decodedPWSettings = encryption.decode(DataAsString.toLatin1(), (m_password + "xtrabytesxtrabytes").toLatin1());
            int posPW = decodedPWSettings.lastIndexOf(QChar('}')); // find last bracket to mark the end of the json
            decodedPWSettings = decodedPWSettings.left(posPW+1); //remove everything after the valid json
            int beginPW = decodedPWSettings.indexOf(QChar('{')); // find first bracket to mark beginning of the json
            qDebug() << "first { at poistion: " << beginPW;
            QJsonObject decodedPWJson = QJsonDocument::fromJson(decodedPWSettings).object();

            if(decodedPWJson.value("app").toString().startsWith("xtrabytes")){
                qDebug() << "valid encryption";
                QVariantMap feed3;
                feed3.insert("randNumAes", randNumAes.second);
                feed3.insert("randNumPass", encodedRandNrStr);
                feed3.insert("sessionIdAes", sessionIdAes.second);
                feed3.insert("settings",DataAsString);
                feed3.insert("username",m_username);

                QByteArray changePassword =  QJsonDocument::fromVariant(feed3).toJson(QJsonDocument::Compact);

                changePassword = changePassword.toBase64();

                // Send new encrypted Rand Nums to backend
                QString changePasswordResponse = RestAPIPostCall("/v1/changePassword", changePassword);
                if (changePasswordResponse.isEmpty()){
                    return;
                }
                QJsonDocument jsonResponseSave = QJsonDocument::fromJson(changePasswordResponse.toLatin1());
                QJsonValue encryptedTextLogin = jsonResponseSave.object().value("login");

                bool changePasswordSuccess = encryptedTextLogin.toString() == "success" ? true:false;

                if (changePasswordSuccess){
                    qDebug() << "valid encrypted settings file";
                    /*      back up encrypted file to device     */
                    BackupFile(m_username.toLower() + ".backup", DataAsString, "restore");

                    QString dec_pincode = encryption.decode(m_pincode.toLatin1(), (oldPassword + "xtrabytesxtrabytes").toLatin1());
                    dec_pincode.chop(1);

                    qDebug() << dec_pincode;


                    QByteArray enc_pincode = encryption.encode((dec_pincode).toLatin1(), (m_password + "xtrabytesxtrabytes").toLatin1());
                    m_pincode = QString::fromLatin1(enc_pincode, enc_pincode.length());

                    emit saveSucceeded();
                    return;
                }
                else {
                    emit passwordChangedFailed();
                    return;
                }
            }
            else {
                emit passwordChangedFailed();
                return;
            }
        }else{
            emit passwordChangedFailed();
            return;
        }
    }
    else {
        qDebug() << "no connection to account server to change password";
        emit passwordChangedFailed();
        emit noInternet();
        return;
    }
}

std::pair<int, QByteArray> Settings::encryptAes(QString text,  unsigned char *key,  unsigned char *iv) {
    EVP_CIPHER_CTX *ctx;
    std::pair<int,QByteArray>  returnVals;
    unsigned char* ciphertext = new unsigned char[32];
    unsigned char* plaintext = new unsigned char[text.size()];
    std::memcpy(plaintext,text.toLatin1().constData(),text.size());
    int plaintext_len = strlen ((char *)plaintext);
    int len;
    int ciphertext_len;

    /* Create and initialise the context */
    if(!(ctx = EVP_CIPHER_CTX_new())) {
        int error = 0;
        qDebug() << "error: ";
        qDebug() << error;
    }

    /* Initialise the encryption operation. IMPORTANT - ensure you use a key
   * and IV size appropriate for your cipher
   * In this example we are using 256 bit AES (i.e. a 256 bit key). The
   * IV size for *most* modes is the same as the block size. For AES this
   * is 128 bits */
    if(1 != EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv)){
        int error = 0;
        qDebug() << "error: ";
        qDebug() << error;
    }

    /* Provide the message to be encrypted, and obtain the encrypted output.
   * EVP_EncryptUpdate can be called multiple times if necessary
   */
    if(1 != EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, plaintext_len)){
        int error = 0;
        qDebug() << "error: ";
        qDebug() << error;
    }
    ciphertext_len = len;

    /* Finalise the encryption. Further ciphertext bytes may be written at
   * this stage.
   */
    if(1 != EVP_EncryptFinal_ex(ctx, ciphertext + len, &len)){
        int error = 0;
        qDebug() << "error: ";
        qDebug() << error;
    }
    ciphertext_len += len;

    QByteArray encryptedAES;
    encryptedAES = QByteArray(reinterpret_cast<char*>(ciphertext), ciphertext_len);

    returnVals.first = ciphertext_len;
    returnVals.second = encryptedAES;

    //  int test = encryptedAES.size();
    /* Clean up */
    EVP_CIPHER_CTX_free(ctx);


    BIO_dump_fp (stdout, (const char *)ciphertext, ciphertext_len);
    delete [] plaintext;
    delete [] ciphertext;


    return returnVals;
}

bool Settings::SaveSettings(){
    static QMutex mutex;
    if (mutex.tryLock()){
        try {
            if (saveSettingsQueue.size() > 0){
                saveSettingsQueue.dequeue();
            }
            if (checkInternet("http://37.59.57.212:8080")) {
                QAESEncryption encryption(QAESEncryption::AES_128, QAESEncryption::ECB);
                QVariantMap settings;

                // Encrypt sessionId with backend key
                std::pair<int, QByteArray> sessionIdAes = encryptAes(sessionId,backendKey, iiiv);
                sessionIdAes.second = sessionIdAes.second.toBase64();
                foreach (const QString &key, m_settings->childKeys()) {//iterate through m_settings to add everything to settings file we write to DB
                    settings.insert(key,m_settings->value(key).toString());
                }
                QString dec_pincode = encryption.decode(m_pincode.toLatin1(), (m_password + "xtrabytesxtrabytes").toLatin1());
                settings.insert("pincode", dec_pincode); //may be able to remove this

                /*      Add contacts to DB       */
                QJsonArray contactsArray = QJsonDocument::fromJson(m_contacts.toLatin1()).array();
                settings.insert("contacts",contactsArray.toVariantList()); // add contacts array to our existing settings

                /*      Add addresses to DB       */
                QJsonArray addressesArray = QJsonDocument::fromJson(m_addresses.toLatin1()).array(); //save addresses saves array to m_addresses
                settings.insert("addresses",addressesArray.toVariantList()); // add address array to our existing settings

                /*      Add pendingTransactions to DB       */
                QJsonArray pendingArray = QJsonDocument::fromJson(m_pending.toLatin1()).array();
                settings.insert("pendingList",pendingArray.toVariantList()); // add pendingTransactions array to our existing settings

                /*      Add wallets to DB       */
                bool localKeys = m_settings->value("localKeys").toBool();
                if (!localKeys){
                    QJsonArray walletArray = QJsonDocument::fromJson(m_wallet.toLatin1()).array(); //savewallet saves array to m_wallet
                    settings.insert("walletList",walletArray.toVariantList()); // add wallet array to our existing settings
                }

                /*    Convert Settings Variant to QByteArray  and encode it   */
                QByteArray settingsOutput =  QJsonDocument::fromVariant(QVariant(settings)).toJson(QJsonDocument::Compact); //Convert settings to byteArray/Json
                QByteArray encodedText = encryption.encode(settingsOutput, (m_password + "xtrabytesxtrabytes").toLatin1()); //encode settings after adding address
                QString DataAsString = QString::fromLatin1(encodedText, encodedText.length());

                /*      check encryption    */
                QByteArray decodedSaveSettings = encryption.decode(DataAsString.toLatin1(), (m_password + "xtrabytesxtrabytes").toLatin1());
                int posSave = decodedSaveSettings.lastIndexOf(QChar('}')); // find last bracket to mark the end of the json
                decodedSaveSettings = decodedSaveSettings.left(posSave+1); //remove everything after the valid json
                int beginSave = decodedSaveSettings.indexOf(QChar('{')); // find first bracket to mark beginning of the json
                qDebug() << "first { at poistion: " << beginSave;
                QJsonObject decodedSaveJson = QJsonDocument::fromJson(decodedSaveSettings).object();

                if(decodedSaveJson.value("app").toString().startsWith("xtrabytes")){
                    qDebug() << "valid encrypted settings file";
                    /*      back up encrypted file to device     */
                    BackupFile(m_username.toLower() + ".backup", DataAsString, "restore");

                    /*      updating encrypted settings     */
                    QVariantMap feed3;
                    feed3.insert("sessionIdAes", sessionIdAes.second);
                    feed3.insert("username",m_username);
                    feed3.insert("settings", DataAsString);

                    QByteArray finalLogin =  QJsonDocument::fromVariant(feed3).toJson(QJsonDocument::Compact);
                    finalLogin = finalLogin.toBase64();
                    // Send sessionId + settings to backend to save
                    QString saveSettingsResponse = RestAPIPostCall("/v1/saveSettings", finalLogin);
                    mutex.unlock();

                    if (saveSettingsResponse.isEmpty()){
                        if (saveSettingsQueue.size() > 0){
                            SaveSettings();
                        }
                        return false;
                    }

                    QJsonDocument jsonResponse = QJsonDocument::fromJson(saveSettingsResponse.toLatin1());
                    QJsonValue encryptedText = jsonResponse.object().value("login");
                    bool settingsSavedSuccess = encryptedText.toString() == "success" ? true:false;

                    if (settingsSavedSuccess){
                        m_oldPincode = m_pincode;
                        emit saveSucceeded();
                    }else{
                        m_pincode = m_oldPincode;
                        emit saveFailed();
                    }
                    if (saveSettingsQueue.size() > 0){
                        SaveSettings();
                    }
                    return true;
                }
                else {
                    qDebug() << "corrupted settings file";
                    m_pincode = m_oldPincode;
                    emit saveFailed();
                    mutex.unlock();

                    if (saveSettingsQueue.size() > 0){
                        SaveSettings();
                    }
                    return false;
                }
            }
            else {
                qDebug() << "no connection to account server to save settings";
                m_pincode = m_oldPincode;
                emit saveFailed();
                emit noInternet();
                mutex.unlock();

                if (saveSettingsQueue.size() > 0){
                    SaveSettings();
                }
                return false;
            }
        } catch (QException e) {
            qDebug() << "exception caught";
            m_pincode = m_oldPincode;
            emit saveFailed();
            mutex.unlock();

            if (saveSettingsQueue.size() > 0){
                SaveSettings();
            }
        }
    }
    else{
        saveSettingsQueue.append(++settingsCount);
    }
    return true;
}

void Settings::LoadSettings(QByteArray settings, QString fileLocation){
    QAESEncryption encryption(QAESEncryption::AES_128, QAESEncryption::ECB);

    QJsonObject json = QJsonDocument::fromJson(settings).object();
    QVariantMap settingsMap;
    m_settings->clear();
    foreach(const QString& key, json.keys()) {
        QJsonValue value = json.value(key);
        settingsMap.insert(key,value.toString());
        m_settings->setValue(key,value.toString());
        m_settings->sync();
    }
    emit settingsLoaded(settingsMap);

    /* Load contacts from JSON from DB */
    QJsonArray contactArray = json["contacts"].toArray(); //get contactList from settings from DB
    QJsonDocument docContact;
    docContact.setArray(contactArray);
    QString contacts(docContact.toJson(QJsonDocument::Compact));
    m_contacts.clear();
    m_contacts = contacts;
    emit contactsLoaded(m_contacts);

    /* Load addresses from JSON from DB */
    QJsonArray addressArray = json["addresses"].toArray(); //get contactList from settings from DB
    QJsonDocument doc;
    doc.setArray(addressArray);
    QString addresses(doc.toJson(QJsonDocument::Compact));
    m_addresses.clear();
    m_addresses = addresses;
    emit addressesLoaded(m_addresses);

    /* Load pendingTransactions from JSON from DB */
    QJsonArray pendingArray = json["pendingList"].toArray(); //get pending transaction list from settings from DB
    QJsonDocument docPending;
    docPending.setArray(pendingArray);
    QString pending(docPending.toJson(QJsonDocument::Compact));
    m_pending.clear();
    m_pending = pending;
    emit pendingLoaded(m_pending);
    int pendingCount = pendingArray.count();
    for (int e = 0; e < pendingCount; e++) {
        qDebug() << pendingArray.at(e);
    }

    /* Load wallets from JSON from Local or DB */
    bool localKeys = m_settings->value("localKeys").toBool();
    bool setupComplete = m_settings->value("accountCreationCompleted").toBool();
    QJsonArray walletArray;
    m_wallet.clear();

    if (localKeys){
        //qDebug().noquote() << "local wallet";
        QString walletFile = LoadFile(m_username.toLower() + ".wallet", fileLocation);
        if (walletFile != "ERROR"){
            QByteArray decodedWallet = encryption.decode(walletFile.toLatin1(), (m_password + "xtrabytesxtrabytes").toLatin1());
            int pos = decodedWallet.lastIndexOf(QChar(']')); // find last bracket to mark the end of the json
            decodedWallet = decodedWallet.left(pos+1); //remove everything after the valid json
            walletArray = QJsonDocument::fromJson(decodedWallet).array();
        }else{
            if (setupComplete) {
                emit walletNotFound();
                qDebug() << "No wallet file found";
                return;
            }
        }
    }else{
        walletArray = json["walletList"].toArray(); //get walletList from settings from DB
    }
    /*
    int walletCount = walletArray.count();
    for (int e = 0; e < walletCount; e++) {
        qDebug() << walletArray.at(e);
    }
    */
    QJsonDocument docWallet;
    docWallet.setArray(walletArray);
    QString wallet(docWallet.toJson(QJsonDocument::Compact));
    m_wallet = wallet;
    emit walletLoaded(m_wallet);

    QString pincode = json.value("pincode").toString().toLatin1();
    QByteArray enc_pincode = encryption.encode( pincode.toLatin1(), (m_password + "xtrabytesxtrabytes").toLatin1());
    m_pincode = QString::fromLatin1(enc_pincode, enc_pincode.length()); //encryption.encode((QString("<xtrabytes>") + pincode).toLatin1(), (m_password + "xtrabytesxtrabytes").toLatin1());
    m_oldPincode = QString::fromLatin1(enc_pincode, enc_pincode.length());

    emit loginSucceededChanged();
    emit xchatConnectedLogin(m_username,"addToOnline","online");
}

void Settings::UpdateAccount(QString addresslist, QString contactlist, QString walletlist, QString pendinglist){
    bool checkPermission = true;
#ifdef Q_OS_ANDROID //added to get write permission for Android
    auto  result = QtAndroid::checkPermission(QString("android.permission.WRITE_EXTERNAL_STORAGE"));
    if(result == QtAndroid::PermissionResult::Denied){
        QtAndroid::PermissionResultMap resultHash = QtAndroid::requestPermissionsSync(QStringList({"android.permission.WRITE_EXTERNAL_STORAGE"}));
        if(resultHash["android.permission.WRITE_EXTERNAL_STORAGE"] == QtAndroid::PermissionResult::Denied){
            checkPermission = false;
        }
    }
#endif
    QAESEncryption encryption(QAESEncryption::AES_128, QAESEncryption::ECB);
    bool localKeys = m_settings->value("localKeys").toBool();
    m_addresses = addresslist;
    m_contacts = contactlist;
    m_wallet = walletlist;
    m_pending = pendinglist;
    if (checkPermission) {

        QString currentDir = QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation)[0] + "/XCITE_backup";
        QString fileName = m_username.toLower() + ".backup";
        QDir dir;
        dir.mkpath(currentDir);
        qDebug() << " Writing to " + currentDir + "/" + fileName;
        QString fullFileName = currentDir + "/" + fileName;
        bool exists = QFile::exists(fullFileName);
        if (!exists) {
            QFile file(currentDir + "/" + fileName);
        }
    }

    if (localKeys){
        if (checkPermission) {

            QString currentDir = QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation)[0] + "/XCITE_wallet";
            QString fileName = m_username.toLower() + ".wallet";
            QDir dir;
            dir.mkpath(currentDir);
            qDebug() << " Writing to " + currentDir + "/" + fileName;
            QString fullFileName = currentDir + "/" + fileName;
            bool walletExists = QFile::exists(fullFileName);
            if (!walletExists) {
                QFile file(currentDir + "/" + fileName);
            }
        }
        QByteArray encodedWallet = encryption.encode(walletlist.toLatin1(), (m_password + "xtrabytesxtrabytes").toLatin1()); //encode settings after adding address
        QString encodedWalletString = QString::fromLatin1(encodedWallet,encodedWallet.length());

        QByteArray decodedWallet = encryption.decode(encodedWalletString.toLatin1(), (m_password + "xtrabytesxtrabytes").toLatin1());
        int pos = decodedWallet.lastIndexOf(QChar(']')); // find last bracket to mark the end of the json
        decodedWallet = decodedWallet.left(pos+1); //remove everything after the valid json
        QString walletString = QString::fromStdString(decodedWallet.toStdString());

        if (walletString == walletlist) {
            SaveFile(m_username.toLower() + ".wallet", encodedWalletString, "default");
        }
        else {
            emit saveFileFailed();
        }
    }else{
        SaveSettings();
    }
}

void Settings::SaveAddresses(QString addresslist){
    bool checkPermission = true;
#ifdef Q_OS_ANDROID //added to get write permission for Android
    auto  result = QtAndroid::checkPermission(QString("android.permission.WRITE_EXTERNAL_STORAGE"));
    if(result == QtAndroid::PermissionResult::Denied){
        QtAndroid::PermissionResultMap resultHash = QtAndroid::requestPermissionsSync(QStringList({"android.permission.WRITE_EXTERNAL_STORAGE"}));
        if(resultHash["android.permission.WRITE_EXTERNAL_STORAGE"] == QtAndroid::PermissionResult::Denied){
            checkPermission = false;
        }
    }
#endif
    if (checkPermission) {

        QString currentDir = QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation)[0] + "/XCITE_backup";
        QString fileName = m_username.toLower() + ".backup";
        QDir dir;
        dir.mkpath(currentDir);
        qDebug() << " Writing to " + currentDir + "/" + fileName;
        QString fullFileName = currentDir + "/" + fileName;
        bool exists = QFile::exists(fullFileName);
        if (!exists) {
            QFile file(currentDir + "/" + fileName);
        }
    }
    m_addresses = addresslist;
    SaveSettings();
}

void Settings::SaveContacts(QString contactlist){
    bool checkPermission = true;
#ifdef Q_OS_ANDROID //added to get write permission for Android
    auto  result = QtAndroid::checkPermission(QString("android.permission.WRITE_EXTERNAL_STORAGE"));
    if(result == QtAndroid::PermissionResult::Denied){
        QtAndroid::PermissionResultMap resultHash = QtAndroid::requestPermissionsSync(QStringList({"android.permission.WRITE_EXTERNAL_STORAGE"}));
        if(resultHash["android.permission.WRITE_EXTERNAL_STORAGE"] == QtAndroid::PermissionResult::Denied){
            checkPermission = false;
        }
    }
#endif
    if (checkPermission) {

        QString currentDir = QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation)[0] + "/XCITE_backup";
        QString fileName = m_username.toLower() + ".backup";
        QDir dir;
        dir.mkpath(currentDir);
        qDebug() << " Writing to " + currentDir + "/" + fileName;
        QString fullFileName = currentDir + "/" + fileName;
        bool exists = QFile::exists(fullFileName);
        if (!exists) {
            QFile file(currentDir + "/" + fileName);
        }
    }
    m_contacts = contactlist;
    SaveSettings();
}

void Settings::SaveWallet(QString walletlist, QString addresslist){
    bool checkPermission = true;
#ifdef Q_OS_ANDROID //added to get write permission for Android
    auto  result = QtAndroid::checkPermission(QString("android.permission.WRITE_EXTERNAL_STORAGE"));
    if(result == QtAndroid::PermissionResult::Denied){
        QtAndroid::PermissionResultMap resultHash = QtAndroid::requestPermissionsSync(QStringList({"android.permission.WRITE_EXTERNAL_STORAGE"}));
        if(resultHash["android.permission.WRITE_EXTERNAL_STORAGE"] == QtAndroid::PermissionResult::Denied){
            checkPermission = false;
        }
    }
#endif
    if (checkPermission) {

        QString currentDir = QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation)[0] + "/XCITE_backup";
        QString fileName = m_username.toLower() + ".backup";
        QDir dir;
        dir.mkpath(currentDir);
        qDebug() << " Writing to " + currentDir + "/" + fileName;
        QString fullFileName = currentDir + "/" + fileName;
        bool exists = QFile::exists(fullFileName);
        if (!exists) {
            QFile file(currentDir + "/" + fileName);
        }
    }

    QAESEncryption encryption(QAESEncryption::AES_128, QAESEncryption::ECB);
    bool localKeys = m_settings->value("localKeys").toBool();
    m_wallet = walletlist;
    m_addresses = addresslist;

    if (localKeys){

        if (checkPermission) {

            QString currentDir = QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation)[0] + "/XCITE_wallet";
            QString fileName = m_username.toLower() + ".wallet";
            QDir dir;
            dir.mkpath(currentDir);
            qDebug() << " Writing to " + currentDir + "/" + fileName;
            QString fullFileName = currentDir + "/" + fileName;
            bool walletExists = QFile::exists(fullFileName);
            if (!walletExists) {
                QFile file(currentDir + "/" + fileName);
            }

            QByteArray encodedWallet = encryption.encode(walletlist.toLatin1(), (m_password + "xtrabytesxtrabytes").toLatin1()); //encode settings after adding address
            QString encodedWalletString = QString::fromLatin1(encodedWallet,encodedWallet.length());

            QByteArray decodedWallet = encryption.decode(encodedWalletString.toLatin1(), (m_password + "xtrabytesxtrabytes").toLatin1());
            int pos = decodedWallet.lastIndexOf(QChar(']')); // find last bracket to mark the end of the json
            decodedWallet = decodedWallet.left(pos+1); //remove everything after the valid json
            QString walletString = QString::fromStdString(decodedWallet.toStdString());

            if (walletString == walletlist) {
                SaveFile(m_username.toLower() + ".wallet", encodedWalletString, "default");
            }
            else {
                emit saveFileFailed();
            }
        }
        else {
            emit saveFileFailed();
        }
    }else{
        SaveSettings();
    }
}

void Settings::ExportWallet(QString walletlist){
    bool checkPermission = true;
#ifdef Q_OS_ANDROID //added to get write permission for Android
    auto  result = QtAndroid::checkPermission(QString("android.permission.WRITE_EXTERNAL_STORAGE"));
    if(result == QtAndroid::PermissionResult::Denied){
        QtAndroid::PermissionResultMap resultHash = QtAndroid::requestPermissionsSync(QStringList({"android.permission.WRITE_EXTERNAL_STORAGE"}));
        if(resultHash["android.permission.WRITE_EXTERNAL_STORAGE"] == QtAndroid::PermissionResult::Denied){
            checkPermission = false;
        }
    }
#endif
    if (checkPermission) {

        QString currentDir = QStandardPaths::standardLocations(QStandardPaths::DownloadLocation)[0] + "/XCITE_wallet";
        QString fileName = m_username.toLower() + ".wallet";
        QDir dir;
        dir.mkpath(currentDir);
        qDebug() << " Writing to " + currentDir + "/" + fileName;
        QString fullFileName = currentDir + "/" + fileName;
        bool exists = QFile::exists(fullFileName);
        if (!exists) {
            QFile file(currentDir + "/" + fileName);
        }
    }
    QAESEncryption encryption(QAESEncryption::AES_128, QAESEncryption::ECB);
    QByteArray encodedWallet = encryption.encode(walletlist.toLatin1(), (m_password + "xtrabytesxtrabytes").toLatin1()); //encode settings after adding address
    QString encodedWalletString = QString::fromLatin1(encodedWallet,encodedWallet.length());
    SaveFile(m_username.toLower() + ".wallet", encodedWalletString, "export");
}

void Settings::initialisePincode(QString pincode){
    QAESEncryption encryption(QAESEncryption::AES_128, QAESEncryption::ECB);
    QByteArray enc_pincode = encryption.encode((QString("<xtrabytes>") + pincode).toLatin1(), (m_password + "xtrabytesxtrabytes").toLatin1());
    m_pincode = QString::fromLatin1(enc_pincode, enc_pincode.length()); //encryption.encode((QString("<xtrabytes>") + pincode).toLatin1(), (m_password + "xtrabytesxtrabytes").toLatin1());
}

void Settings::onSavePincode(QString pincode){
    bool checkPermission = true;
#ifdef Q_OS_ANDROID //added to get write permission for Android
    auto  result = QtAndroid::checkPermission(QString("android.permission.WRITE_EXTERNAL_STORAGE"));
    if(result == QtAndroid::PermissionResult::Denied){
        QtAndroid::PermissionResultMap resultHash = QtAndroid::requestPermissionsSync(QStringList({"android.permission.WRITE_EXTERNAL_STORAGE"}));
        if(resultHash["android.permission.WRITE_EXTERNAL_STORAGE"] == QtAndroid::PermissionResult::Denied){
            checkPermission = false;
        }
    }
#endif
    if (checkPermission) {

        QString currentDir = QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation)[0] + "/XCITE_backup";
        QString fileName = m_username.toLower() + ".backup";
        QDir dir;
        dir.mkpath(currentDir);
        qDebug() << " Writing to " + currentDir + "/" + fileName;
        QString fullFileName = currentDir + "/" + fileName;
        bool exists = QFile::exists(fullFileName);
        if (!exists) {
            QFile file(currentDir + "/" + fileName);
        }
    }
    QAESEncryption encryption(QAESEncryption::AES_128, QAESEncryption::ECB);
    QByteArray enc_pincode = encryption.encode((QString("<xtrabytes>") + pincode).toLatin1(), (m_password + "xtrabytesxtrabytes").toLatin1());
    m_pincode = QString::fromLatin1(enc_pincode, enc_pincode.length()); //encryption.encode((QString("<xtrabytes>") + pincode).toLatin1(), (m_password + "xtrabytesxtrabytes").toLatin1());
    SaveSettings();
}

bool Settings::checkPincode(QString pincode){
    QAESEncryption encryption(QAESEncryption::AES_128, QAESEncryption::ECB);
    QString pincodeKey = QString("<xtrabytes>") + pincode;

    QString dec_pincode = encryption.decode(m_pincode.toLatin1(), (m_password + "xtrabytesxtrabytes").toLatin1());
    dec_pincode.chop(1);

    if (pincodeKey == dec_pincode){
        emit pincodeCorrect();
        return true;
    }else{
        emit pincodeFalse();
        return false;
    }
}

QString Settings::CheckStatusCode(QString statusCode){
    QString returnVal;
    if (statusCode.startsWith("2")){
        returnVal = "Success";
    }else if (statusCode.startsWith("3")) {
        returnVal = "API Connection Error";
        emit saveFailedAPIError();
    }else if (statusCode.startsWith("4")) {
        returnVal = "Input Error";
        emit saveFailedInputError();
    }else if (statusCode.startsWith("5")) {
        returnVal = "DB Error";
        emit saveFailedDBError();
    }else{
        returnVal = "Unknown Error";
        emit saveFailedUnknownError();
    }
    return returnVal;
}

void Settings::BackupFile(QString fileName, QString encryptedData, QString fileLocation){

    bool checkPermission = true;
#ifdef Q_OS_ANDROID //added to get write permission for Android
    auto  result = QtAndroid::checkPermission(QString("android.permission.WRITE_EXTERNAL_STORAGE"));
    if(result == QtAndroid::PermissionResult::Denied){
        QtAndroid::PermissionResultMap resultHash = QtAndroid::requestPermissionsSync(QStringList({"android.permission.WRITE_EXTERNAL_STORAGE"}));
        if(resultHash["android.permission.WRITE_EXTERNAL_STORAGE"] == QtAndroid::PermissionResult::Denied){
            checkPermission = false;
        }
    }
#endif
    if (checkPermission) {
        QString currentDir = QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation)[0] + "/XCITE_backup";
        QDir dir;
        dir.mkpath(currentDir);
        qDebug() << " Writing to " + currentDir + "/" + fileName;
        QFile file(currentDir + "/" + fileName);
        if (!file.open(QIODevice::WriteOnly)) {
            file.errorString(); //We can build this out to emit an error message
            qDebug() << file.errorString();
            return;
        }else{
            QDataStream out(&file);
            out.setVersion(QDataStream::Qt_5_11);
            out << encryptedData;
            qDebug() << "backup file saved";
            file.close();
        }
    }
}

void Settings::SaveFile(QString fileName, QString encryptedData, QString fileLocation){

    bool checkPermission = true;
#ifdef Q_OS_ANDROID //added to get write permission for Android
    auto  result = QtAndroid::checkPermission(QString("android.permission.WRITE_EXTERNAL_STORAGE"));
    if(result == QtAndroid::PermissionResult::Denied){
        QtAndroid::PermissionResultMap resultHash = QtAndroid::requestPermissionsSync(QStringList({"android.permission.WRITE_EXTERNAL_STORAGE"}));
        if(resultHash["android.permission.WRITE_EXTERNAL_STORAGE"] == QtAndroid::PermissionResult::Denied){
            emit saveFileFailed();
            checkPermission = false;
        }
    }
#endif
    if (checkPermission) {
        QString currentDir = QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation)[0] + "/XCITE_wallet";
        if (fileLocation == "export"){
            currentDir = QStandardPaths::standardLocations(QStandardPaths::DownloadLocation)[0];
        }
        QDir dir;
        dir.mkpath(currentDir);
        qDebug() << " Writing to " + currentDir + "/" + fileName;
        QFile file(currentDir + "/" + fileName);
        if (!file.open(QIODevice::WriteOnly)) {
            file.errorString(); //We can build this out to emit an error message
            qDebug() << file.errorString();
            emit saveFileFailed();
            return;
        }else{
            QDataStream out(&file);
            out.setVersion(QDataStream::Qt_5_11);
            out << encryptedData;
            qDebug() << "wallet file saved";
            file.close();
            emit saveFileSucceeded();
            SaveSettings();
        }
    }
}

QString Settings::LoadFile(QString fileName, QString fileLocation){

#ifdef Q_OS_ANDROID //added to get write permission for Android
    auto  result = QtAndroid::checkPermission(QString("android.permission.WRITE_EXTERNAL_STORAGE"));
    if(result == QtAndroid::PermissionResult::Denied){
        QtAndroid::PermissionResultMap resultHash = QtAndroid::requestPermissionsSync(QStringList({"android.permission.WRITE_EXTERNAL_STORAGE"}));
        if(resultHash["android.permission.WRITE_EXTERNAL_STORAGE"] == QtAndroid::PermissionResult::Denied){
            return "ERROR";
        }
    }
#endif
    QString returnFile;

    QString currentDir = QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation)[0] + "/XCITE_wallet";
    if (fileLocation == "import"){
        currentDir = QStandardPaths::standardLocations(QStandardPaths::DownloadLocation)[0];
    }
    else if (fileLocation == "restore") {
        currentDir = QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation)[0] + "/XCITE_backup";
    }

    qDebug() << "Reading from " + currentDir + "/" + fileName;

    QFile file(currentDir + "/" + fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        file.errorString(); //We can build this out to emit an error message
        if (fileLocation == "restore") {
            NoBackupFile();
        }
        else if (fileLocation == "import") {
            NoImportFile();
        }
        else {
            NoWalletFile();
        }
        return "ERROR";
    }
    else {
        QDataStream in(&file);
        in.setVersion(QDataStream::Qt_5_11);
        in >> returnFile;
        file.close();
        return returnFile;
    }
}

void Settings::CheckSessionId(){

    qDebug() << "checking session ID";
    if (checkInternet("http://37.59.57.212:8080")) {
        // Encrypt sessionId with backend key
        std::pair<int, QByteArray> sessionIdAes = encryptAes(sessionId, backendKey, iiiv);
        sessionIdAes.second = sessionIdAes.second.toBase64();

        QVariantMap feed;
        feed.insert("sessionIdAes", sessionIdAes.second);
        feed.insert("username",m_username);

        QByteArray checkSession =  QJsonDocument::fromVariant(feed).toJson(QJsonDocument::Compact);
        checkSession = checkSession.toBase64();

        // Send sessionId + settings to backend to save
        QString checkSessionResponse = RestAPIPostCall("/v1/checkSessionId", checkSession);
        if (checkSessionResponse.isEmpty()){
            return;
        }
        bool sessionID = checkSessionResponse.contains("sessionId");
        if (sessionID) {
            QJsonDocument jsonResponse = QJsonDocument::fromJson(checkSessionResponse.toLatin1());
            QJsonValue encryptedText = jsonResponse.object().value("sessionId");
            QString sessionCheckBool = encryptedText.toString();
            emit sessionIdCheck(sessionCheckBool);
        }
        else {
            emit sessionIdCheck("no_response");
        }
    }
    else {
        emit sessionIdCheck("no_internet");
    }
}

void Settings::NoWalletFile(){

    qDebug() << "No wallet file for account on device!";

    QMessageBox *msgBox = new QMessageBox;
    msgBox->setParent(0);
    msgBox->setWindowTitle("Wallet file ERROR!!!");
    msgBox->setIcon(QMessageBox::Warning);
    msgBox->setText("No wallet file was found");
    msgBox->setStandardButtons(QMessageBox::Ok);
    msgBox->setWindowFlags(Qt::FramelessWindowHint|Qt::WindowStaysOnTopHint);
    msgBox->show();
}

void Settings::NoImportFile(){

    qDebug() << "No wallet file for account in Import location!";

    QMessageBox *msgBox = new QMessageBox;
    msgBox->setParent(0);
    msgBox->setWindowTitle("Wallet file ERROR!!!");
    msgBox->setIcon(QMessageBox::Warning);
    msgBox->setText("No wallet file was found");
    msgBox->setStandardButtons(QMessageBox::Ok);
    msgBox->setWindowFlags(Qt::FramelessWindowHint|Qt::WindowStaysOnTopHint);
    msgBox->show();
}

void Settings::NoBackupFile(){

    qDebug() << "No backup file found!";

    QMessageBox *msgBox = new QMessageBox;
    msgBox->setParent(0);
    msgBox->setWindowTitle("Backup file ERROR!!!");
    msgBox->setIcon(QMessageBox::Warning);
    msgBox->setText("No backup file was found");
    msgBox->setStandardButtons(QMessageBox::Ok);
    msgBox->setWindowFlags(Qt::FramelessWindowHint|Qt::WindowStaysOnTopHint);
    msgBox->show();
}

void Settings::CheckCamera(){
    bool passed = true;
#ifdef Q_OS_ANDROID //added to get camera permission for Android
    auto  result = QtAndroid::checkPermission(QString("android.permission.CAMERA"));
    qDebug() << "Checking camera permission";
    if(result == QtAndroid::PermissionResult::Denied){
        QtAndroid::PermissionResultMap resultHash = QtAndroid::requestPermissionsSync(QStringList({"android.permission.CAMERA"}));
        if(resultHash["android.permission.CAMERA"] == QtAndroid::PermissionResult::Denied){
            qDebug() << "No camera permission";
            passed = false;
        }else{
            qDebug() << "Camera permission ok";
        }
    }
    else {
        qDebug() << "Camera permission ok";
    }
#endif
    if (passed) {
        emit cameraCheckPassed();
    }
    else {
        emit cameraCheckFailed();
    }
}

void Settings::CheckWriteAccess() {
    bool passed = true;
#ifdef Q_OS_ANDROID //added to get camera permission for Android
    auto  result = QtAndroid::checkPermission(QString("android.permission.WRITE_EXTERNAL_STORAGE"));
    qDebug() << "Checking write permission";
    if(result == QtAndroid::PermissionResult::Denied){
        QtAndroid::PermissionResultMap resultHash = QtAndroid::requestPermissionsSync(QStringList({"android.permission.WRITE_EXTERNAL_STORAGE"}));
        if(resultHash["android.permission.WRITE_EXTERNAL_STORAGE"] == QtAndroid::PermissionResult::Denied){
            qDebug() << "No write permission";
            passed = false;
        }else{
            qDebug() << "Write permission ok";
        }
    }
    else {
        qDebug() << "Write permission ok";
    }
#endif
    if (passed) {
        emit writeCheckPassed();
    }
    else {
        emit writeCheckFailed();
    }
}

void Settings::downloadImage(QString imageUrl) {
    // put download function here
}

bool Settings::checkInternet(QString url){
    bool internetStatus = false;
    QNetworkAccessManager *manager = new QNetworkAccessManager(this);
    if(manager->networkAccessible() == QNetworkAccessManager::Accessible) {
        QTimer timeout;
        QEventLoop loop;
        timeout.setSingleShot(true);
        timeout.start(6000);
        connect(&timeout, SIGNAL(timeout()), &loop, SLOT(quit()),Qt::QueuedConnection);
        auto connectionHandler = connect(this, &Settings::internetStatusSignal, [&internetStatus, &loop](bool checked) {
            internetStatus = checked;
            loop.quit();

        });

        URLObject urlObj {QUrl(url)};
        urlObj.addProperty("route","checkInternetSlot");
        DownloadManagerHandler(&urlObj);

        loop.exec();
        disconnect(connectionHandler);
        timeout.deleteLater();

        return internetStatus;
    }
    else {
        return internetStatus;
    }
}

//SLOTS//
void Settings::userExistsSlot(QByteArray response, QMap<QString,QVariant> props){
    if (response != ""){
        emit userAlreadyExists();
        emit userExistsSignal(true);
    }else{
        emit usernameAvailable();
        emit userExistsSignal(false);
    }
}

void Settings::checkInternetSlot(QByteArray response, QMap<QString,QVariant> props){
    if (response != ""){
        emit internetStatusSignal(true);
    }else{
        emit internetStatusSignal(false);
    }
}

void Settings::apiPostSlot(QByteArray response, QMap<QString,QVariant> props){
    if (response != ""){
        emit apiPostSignal(true,response);
    }else{
        emit apiPostSignal(false,"");
    }
}


void Settings::DownloadManagerHandler(URLObject *url){

    DownloadManager *manager = DownloadManager::getInstance();

    url->addProperty("url",url->getUrl());

    url->addProperty("class","Settings");
    manager->append(url);

    connect(manager,  SIGNAL(readTimeout(QMap<QString,QVariant>)),this,SLOT(internetTimeout(QMap<QString,QVariant>)),Qt::UniqueConnection);

    connect(manager,  SIGNAL(readFinished(QByteArray,QMap<QString,QVariant>)), this,SLOT(DownloadManagerRouter(QByteArray,QMap<QString,QVariant>)),Qt::UniqueConnection);


}
void Settings::internetTimeout(QMap<QString,QVariant> props){

    if (props.value("class").toString() == "Settings"){
        internetActive = false;
        qDebug() << "timeout caught in settings";
    }
}
void Settings::DownloadManagerRouter(QByteArray response, QMap<QString,QVariant> props){
    internetActive = true;

    if (props.value("class").toString() == "Settings"){
        QString route = props.value("route").toString();

        if (route == "userExistsSlot"){
            userExistsSlot(response,props);
        }else if (route == "checkInternetSlot"){
            checkInternetSlot(response,props);
        }else if (route == "apiPostSlot"){
            apiPostSlot(response,props);
        }
        //            }else if (route == "getDetails"){
        //                   getDetailsSlot(response,props);
        //            }else if (route == "getBalanceAddressXBYSlot"){
        //                getBalanceAddressXBYSlot(response,props);
        //            }else if (route == "getBalanceAddressExtSlot"){
        //                getBalanceAddressExtSlot(response,props);
        //            }else if (route == "getTransactionStatusSlot"){
        //                getTransactionStatusSlot(response,props);
        //            }
    }
}
