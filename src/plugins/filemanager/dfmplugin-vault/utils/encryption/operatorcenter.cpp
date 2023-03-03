// SPDX-FileCopyrightText: 2022 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "operatorcenter.h"
#include "vaultconfig.h"
#include "utils/operator/pbkdf2.h"
#include "utils/operator/rsam.h"

#include <dfm-io/dfmio_utils.h>

#include <dfm-framework/dpf.h>

#include <QDir>
#include <QDebug>
#include <QPixmap>
#include <QPainter>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QTime>
#include <QtGlobal>
#include <QProcess>

using namespace dfmplugin_vault;
OperatorCenter::OperatorCenter(QObject *parent)
    : QObject(parent), strCryfsPassword(""), strUserKey(""), standOutput("")
{
}

QString OperatorCenter::makeVaultLocalPath(const QString &before, const QString &behind)
{
    return DFMIO::DFMUtils::buildFilePath(kVaultBasePath.toStdString().c_str(),
                                          before.toStdString().c_str(),
                                          behind.toStdString().c_str(), nullptr);
}

bool OperatorCenter::runCmd(const QString &cmd)
{
    QProcess process;
    int mescs = 10000;
    if (cmd.startsWith(kRootProxy)) {
        mescs = -1;
    }
    process.start(cmd);

    bool res = process.waitForFinished(mescs);
    standOutput = process.readAllStandardOutput();
    int exitCode = process.exitCode();
    if (cmd.startsWith(kRootProxy) && (exitCode == 127 || exitCode == 126)) {
        QString strOut = "Run \'" + cmd + "\' fauled: Password Error! " + QString::number(exitCode) + "\n";
        qDebug() << strOut;
        return false;
    }

    if (res == false) {
        QString strOut = "Run \'" + cmd + "\' failed\n";
        qDebug() << strOut;
    }

    return res;
}

bool OperatorCenter::executeProcess(const QString &cmd)
{
    if (false == cmd.startsWith("sudo")) {
        return runCmd(cmd);
    }

    runCmd("id -un");
    if (standOutput.trimmed() == "root") {
        return runCmd(cmd);
    }

    QString newCmd = QString(kRootProxy) + " \"";
    newCmd += cmd;
    newCmd += "\"";
    newCmd.remove("sudo");
    return runCmd(newCmd);
}

bool OperatorCenter::secondSaveSaltAndCiphertext(const QString &ciphertext, const QString &salt, const char *vaultVersion)
{
    // 密文
    QString strCiphertext = pbkdf2::pbkdf2EncrypyPassword(ciphertext, salt, kIterationTwo, kPasswordCipherLength);
    if (strCiphertext.isEmpty())
        return false;
    // 写入文件
    QString strSaltAndCiphertext = salt + strCiphertext;
    VaultConfig config;
    config.set(kConfigNodeName, kConfigKeyCipher, QVariant(strSaltAndCiphertext));
    // 更新保险箱版本信息
    config.set(kConfigNodeName, kConfigKeyVersion, QVariant(vaultVersion));

    return true;
}

bool OperatorCenter::createKeyNew(const QString &password)
{
    strPubKey.clear();
    QString strPriKey("");
    rsam::createPublicAndPrivateKey(strPubKey, strPriKey);

    // 私钥加密
    QString strCipher = rsam::privateKeyEncrypt(password, strPriKey);

    // 验证公钥长度
    if (strPubKey.length() < 2 * kUserKeyInterceptIndex + 32) {
        qDebug() << "USER_KEY_LENGTH is to long!";
        strPubKey.clear();
        return false;
    }

    // 保存密文
    QString strCipherFilePath = makeVaultLocalPath(kRSACiphertextFileName);
    QFile cipherFile(strCipherFilePath);
    if (!cipherFile.open(QIODevice::Text | QIODevice::WriteOnly | QIODevice::Truncate)) {
        qDebug() << "open rsa cipher file failure!";
        return false;
    }
    QTextStream out2(&cipherFile);
    out2 << strCipher;
    cipherFile.close();

    return true;
}

bool OperatorCenter::saveKey(QString key, QString path)
{
    // 保存部分公钥
    QString publicFilePath = path;
    QFile publicFile(publicFilePath);
    if (!publicFile.open(QIODevice::Text | QIODevice::WriteOnly | QIODevice::Truncate)) {
        qDebug() << "open public key file failure!";
        return false;
    }
    publicFile.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadGroup);
    QTextStream out(&publicFile);
    out << key;
    publicFile.close();
    return true;
}

QString OperatorCenter::getPubKey()
{
    return strPubKey;
}

bool OperatorCenter::verificationRetrievePassword(const QString keypath, QString &password)
{
    QFile localPubKeyfile(keypath);
    if (!localPubKeyfile.open(QIODevice::Text | QIODevice::ReadOnly)) {
        qDebug() << "cant't open local public key file!";
        return false;
    }

    QString strLocalPubKey(localPubKeyfile.readAll());
    localPubKeyfile.close();

    // 利用完整公钥解密密文，得到密码
    QString strRSACipherFilePath = makeVaultLocalPath(kRSACiphertextFileName);
    QFile rsaCipherfile(strRSACipherFilePath);
    if (!rsaCipherfile.open(QIODevice::Text | QIODevice::ReadOnly)) {
        qDebug() << "cant't open rsa cipher file!";
        return false;
    }

    QString strRsaCipher(rsaCipherfile.readAll());
    rsaCipherfile.close();

    password = rsam::publicKeyDecrypt(strRsaCipher, strLocalPubKey);

    // 判断密码的正确性，如果密码正确，则用户密钥正确，否则用户密钥错误
    QString temp = "";
    if (!checkPassword(password, temp)) {
        qDebug() << "user key error!";
        return false;
    }

    return true;
}

OperatorCenter *OperatorCenter::getInstance()
{
    static OperatorCenter instance;
    return &instance;
}

OperatorCenter::~OperatorCenter()
{
}

bool OperatorCenter::createDirAndFile()
{
    // 创建配置文件目录
    QString strConfigDir = makeVaultLocalPath();
    QDir configDir(strConfigDir);
    if (!configDir.exists()) {
        bool ok = configDir.mkpath(strConfigDir);
        if (!ok) {
            qDebug() << "create config dir failure!";
            return false;
        }
    }

    // 创建配置文件,并设置文件权限
    QString strConfigFilePath = strConfigDir + QDir::separator() + kVaultConfigFileName;
    QFile configFile(strConfigFilePath);
    if (!configFile.exists()) {
        // 如果文件不存在，则创建文件，并设置权限
        if (configFile.open(QFileDevice::WriteOnly | QFileDevice::Text)) {
            configFile.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadGroup);
            configFile.close();
        } else {
            qInfo() << "保险箱：创建配置文件失败！";
        }
    }

    // 创建存放rsa公钥的文件,并设置文件权限
    QString strPriKeyFile = makeVaultLocalPath(kRSAPUBKeyFileName);
    QFile prikeyFile(strPriKeyFile);
    if (!prikeyFile.open(QIODevice::WriteOnly | QIODevice::Append)) {
        qDebug() << "create rsa private key file failure!";
        return false;
    }
    prikeyFile.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadGroup);
    prikeyFile.close();

    // 创建存放rsa公钥加密后密文的文件,并设置文件权限
    QString strRsaCiphertext = makeVaultLocalPath(kRSACiphertextFileName);
    QFile rsaCiphertextFile(strRsaCiphertext);
    if (!rsaCiphertextFile.open(QIODevice::WriteOnly | QIODevice::Append)) {
        qDebug() << "create rsa ciphertext file failure!";
        return false;
    }
    rsaCiphertextFile.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadGroup);
    rsaCiphertextFile.close();

    // 创建密码提示信息文件,并设置文件权限
    QString strPasswordHintFilePath = makeVaultLocalPath(kPasswordHintFileName);
    QFile passwordHintFile(strPasswordHintFilePath);
    if (!passwordHintFile.open(QIODevice::WriteOnly | QIODevice::Append)) {
        qDebug() << "create password hint file failure!";
        return false;
    }
    passwordHintFile.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadGroup);
    passwordHintFile.close();

    return true;
}

bool OperatorCenter::savePasswordAndPasswordHint(const QString &password, const QString &passwordHint)
{
    // encrypt password，write salt and cihper to file
    // random salt
    const QString &strRandomSalt = pbkdf2::createRandomSalt(kRandomSaltLength);
    // cipher
    const QString &strCiphertext = pbkdf2::pbkdf2EncrypyPassword(password, strRandomSalt, kIteration, kPasswordCipherLength);
    // salt and cipher
    const QString &strSaltAndCiphertext = strRandomSalt + strCiphertext;
    // save the second encrypt cipher, and update version
    secondSaveSaltAndCiphertext(strSaltAndCiphertext, strRandomSalt, kConfigVaultVersion1050);

    // 保存密码提示信息
    const QString &strPasswordHintFilePath = makeVaultLocalPath(kPasswordHintFileName);
    QFile passwordHintFile(strPasswordHintFilePath);
    if (!passwordHintFile.open(QIODevice::Text | QIODevice::WriteOnly | QIODevice::Truncate)) {
        qDebug() << "write password hint failure";
        return false;
    }
    QTextStream out2(&passwordHintFile);
    out2 << passwordHint;
    passwordHintFile.close();

    VaultConfig config;
    const QString &useUserPassword = config.get(kConfigNodeName, kConfigKeyUseUserPassWord, QVariant(kConfigKeyNotExist)).toString();
    if (useUserPassword != kConfigKeyNotExist) {
        strCryfsPassword = password;
    } else {
        strCryfsPassword = strSaltAndCiphertext;
    }

    return true;
}

bool OperatorCenter::createKey(const QString &password, int bytes)
{
    // 清空上次的用户密钥
    strUserKey.clear();

    // 创建密钥对
    QString strPriKey("");
    QString strPubKey("");
    rsam::createPublicAndPrivateKey(strPubKey, strPriKey);

    // 私钥加密
    QString strCipher = rsam::privateKeyEncrypt(password, strPriKey);

    // 将公钥分成两部分（一部分用于保存到本地，一部分生成二维码，提供给用户）
    QString strSaveToLocal("");
    if (strPubKey.length() < 2 * kUserKeyInterceptIndex + bytes) {
        qDebug() << "USER_KEY_LENGTH is to long!";
        return false;
    }
    QString strPart1 = strPubKey.mid(0, kUserKeyInterceptIndex);
    QString strPart2 = strPubKey.mid(kUserKeyInterceptIndex, kUserKeyLength);
    QString strPart3 = strPubKey.mid(kUserKeyInterceptIndex + kUserKeyLength);
    strUserKey = strPart2;
    strSaveToLocal = strPart1 + strPart3;

    // 保存部分公钥
    QString publicFilePath = makeVaultLocalPath(kRSAPUBKeyFileName);
    QFile publicFile(publicFilePath);
    if (!publicFile.open(QIODevice::Text | QIODevice::WriteOnly | QIODevice::Truncate)) {
        qDebug() << "open public key file failure!";
        return false;
    }
    QTextStream out(&publicFile);
    out << strSaveToLocal;
    publicFile.close();

    // 保存密文
    QString strCipherFilePath = makeVaultLocalPath(kRSACiphertextFileName);
    QFile cipherFile(strCipherFilePath);
    if (!cipherFile.open(QIODevice::Text | QIODevice::WriteOnly | QIODevice::Truncate)) {
        qDebug() << "open rsa cipher file failure!";
        return false;
    }
    QTextStream out2(&cipherFile);
    out2 << strCipher;
    cipherFile.close();

    return true;
}

bool OperatorCenter::checkPassword(const QString &password, QString &cipher)
{
    // 获得版本信息
    VaultConfig config;
    const QString &strVersion = config.get(kConfigNodeName, kConfigKeyVersion).toString();

    if ((kConfigVaultVersion == strVersion) || (kConfigVaultVersion1050 == strVersion)) {   // 如果是新版本，验证第二次加密的结果
        // 获得本地盐及密文
        QString strSaltAndCipher = config.get(kConfigNodeName, kConfigKeyCipher).toString();
        QString strSalt = strSaltAndCipher.mid(0, kRandomSaltLength);
        QString strCipher = strSaltAndCipher.mid(kRandomSaltLength);
        // pbkdf2第一次加密密码,获得密文
        QString strNewCipher = pbkdf2::pbkdf2EncrypyPassword(password, strSalt, kIteration, kPasswordCipherLength);
        // 组合密文和盐值
        QString strNewSaltAndCipher = strSalt + strNewCipher;
        // pbkdf2第二次加密密码，获得密文
        QString strNewCipher2 = pbkdf2::pbkdf2EncrypyPassword(strNewSaltAndCipher, strSalt, kIterationTwo, kPasswordCipherLength);

        if (strCipher != strNewCipher2) {
            qDebug() << "password error!";
            return false;
        }

        const QString &useUserPassword = config.get(kConfigNodeName, kConfigKeyUseUserPassWord, QVariant(kConfigKeyNotExist)).toString();
        if (useUserPassword != kConfigKeyNotExist) {
            cipher = password;
        } else {
            cipher = strNewSaltAndCipher;
        }
    } else {   // 如果是旧版本，验证第一次加密的结果
        // 获得本地盐及密文
        QString strfilePath = makeVaultLocalPath(kPasswordFileName);
        QFile file(strfilePath);
        if (!file.open(QIODevice::Text | QIODevice::ReadOnly)) {
            qDebug() << "open pbkdf2cipher file failure!";
            return false;
        }
        QString strSaltAndCipher = QString(file.readAll());
        file.close();
        QString strSalt = strSaltAndCipher.mid(0, kRandomSaltLength);
        QString strCipher = strSaltAndCipher.mid(kRandomSaltLength);

        // pbkdf2加密密码,获得密文
        QString strNewCipher = pbkdf2::pbkdf2EncrypyPassword(password, strSalt, kIteration, kPasswordCipherLength);
        QString strNewSaltAndCipher = strSalt + strNewCipher;
        if (strNewSaltAndCipher != strSaltAndCipher) {
            qDebug() << "password error!";
            return false;
        }

        cipher = strNewSaltAndCipher;

        // 保存第二次加密后的密文,并更新保险箱版本信息
        if (!secondSaveSaltAndCiphertext(strNewSaltAndCipher, strSalt, kConfigVaultVersion)) {
            qDebug() << "第二次加密密文失败！";
            return false;
        }

        // 删除旧版本遗留的密码文件
        QFile::remove(strfilePath);
    }
    return true;
}

bool OperatorCenter::checkUserKey(const QString &userKey, QString &cipher)
{
    if (userKey.length() != kUserKeyLength) {
        qDebug() << "user key length error!";
        return false;
    }

    // 结合本地公钥和用户密钥，还原完整公钥
    QString strLocalPubKeyFilePath = makeVaultLocalPath(kRSAPUBKeyFileName);
    QFile localPubKeyfile(strLocalPubKeyFilePath);
    if (!localPubKeyfile.open(QIODevice::Text | QIODevice::ReadOnly)) {
        qDebug() << "cant't open local public key file!";
        return false;
    }
    QString strLocalPubKey(localPubKeyfile.readAll());
    localPubKeyfile.close();

    QString strNewPubKey = strLocalPubKey.insert(kUserKeyInterceptIndex, userKey);

    // 利用完整公钥解密密文，得到密码
    QString strRSACipherFilePath = makeVaultLocalPath(kRSACiphertextFileName);
    QFile rsaCipherfile(strRSACipherFilePath);
    if (!rsaCipherfile.open(QIODevice::Text | QIODevice::ReadOnly)) {
        qDebug() << "cant't open rsa cipher file!";
        return false;
    }
    QString strRsaCipher(rsaCipherfile.readAll());
    rsaCipherfile.close();

    QString strNewPassword = rsam::publicKeyDecrypt(strRsaCipher, strNewPubKey);

    // 判断密码的正确性，如果密码正确，则用户密钥正确，否则用户密钥错误
    if (!checkPassword(strNewPassword, cipher)) {
        qDebug() << "user key error!";
        return false;
    }

    return true;
}

QString OperatorCenter::getUserKey()
{
    return strUserKey;
}

bool OperatorCenter::getPasswordHint(QString &passwordHint)
{
    QString strPasswordHintFilePath = makeVaultLocalPath(kPasswordHintFileName);
    QFile passwordHintFile(strPasswordHintFilePath);
    if (!passwordHintFile.open(QIODevice::Text | QIODevice::ReadOnly)) {
        qDebug() << "open password hint file failure";
        return false;
    }
    passwordHint = QString(passwordHintFile.readAll());
    passwordHintFile.close();

    return true;
}

QString OperatorCenter::getSaltAndPasswordCipher()
{
    return strCryfsPassword;
}

void OperatorCenter::clearSaltAndPasswordCipher()
{
    strCryfsPassword.clear();
}

QString OperatorCenter::getEncryptDirPath()
{
    return makeVaultLocalPath(kVaultEncrypyDirName);
}

QString OperatorCenter::getdecryptDirPath()
{
    return makeVaultLocalPath(kVaultDecryptDirName);
}

QStringList OperatorCenter::getConfigFilePath()
{
    QStringList lstPath;

    lstPath << makeVaultLocalPath(kPasswordFileName);
    lstPath << makeVaultLocalPath(kRSAPUBKeyFileName);
    lstPath << makeVaultLocalPath(kRSACiphertextFileName);
    lstPath << makeVaultLocalPath(kPasswordHintFileName);

    return lstPath;
}

QString OperatorCenter::autoGeneratePassword(int length)
{
    if (length < 3) return "";
    qsrand(uint(QTime(0, 0, 0).secsTo(QTime::currentTime())));

    QString strPassword("");

    QString strNum("0123456789");
    strPassword += strNum.at(qrand() % 10);

    QString strSpecialChar("`~!@#$%^&*");
    strPassword += strSpecialChar.at(qrand() % 10);

    QString strABC("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");
    strPassword += strABC.at(qrand() % 10);

    QString strAllChar = strNum + strSpecialChar + strABC;
    int nCount = length - 3;
    for (int i = 0; i < nCount; ++i) {
        strPassword += strAllChar.at(qrand() % 52);
    }
    return strPassword;
}

bool OperatorCenter::getRootPassword()
{
    // 判断当前是否是管理员登陆
    bool res = runCmd("id -un");   // file path is fixed. So write cmd direct
    if (res && standOutput.trimmed() == "root") {
        return true;
    }

    if (false == executeProcess("sudo whoami")) {
        return false;
    }

    return true;
}

int OperatorCenter::executionShellCommand(const QString &strCmd, QStringList &lstShellOutput)
{
    FILE *fp;

    std::string sCmd = strCmd.toStdString();
    const char *cmd = sCmd.c_str();

    // 命令为空
    if (strCmd.isEmpty()) {
        qDebug() << "cmd is empty!";
        return -1;
    }

    if ((fp = popen(cmd, "r")) == nullptr) {
        perror("popen");
        qDebug() << QString("popen error: %s").arg(strerror(errno));
        return -1;
    } else {
        char buf[kBuffterMaxLine] = { '\0' };
        while (fgets(buf, sizeof(buf), fp)) {   // 获得每行输出
            QString strLineOutput(buf);
            if (strLineOutput.endsWith('\n'))
                strLineOutput.chop(1);
            lstShellOutput.push_back(strLineOutput);
        }

        int res;
        if ((res = pclose(fp)) == -1) {
            qDebug() << "close popen file pointer fp error!";
            return res;
        } else if (res == 0) {
            return res;
        } else {
            qDebug() << QString("popen res is : %1").arg(res);
            return res;
        }
    }
}

bool OperatorCenter::savePasswordToKeyring(const QString &password)
{
    return dpfSlotChannel->push("dfmplugin_utils", "slot_VaultHelper_SavePasswordToKeyring", password).toBool();
}

QString OperatorCenter::passwordFromKeyring()
{
    return dpfSlotChannel->push("dfmplugin_utils", "slot_VaultHelper_PasswordFromKeyring").toString();
}