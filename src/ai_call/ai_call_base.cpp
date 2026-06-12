#include "ai_call_base.h"

AICallBase::AICallBase(QObject *parent)
    : QObject(parent)
    , m_lastErrorType(AIErrorType::NoError)
    , m_timeout(30000)  // 默认30秒超时
    , m_timeoutTimer(new QTimer(this))
    , m_currentReply(nullptr)
{
    // 初始化网络管理器
    m_networkManager = new QNetworkAccessManager(this);

    // 连接超时定时器
    m_timeoutTimer->setSingleShot(true);
    connect(m_timeoutTimer, &QTimer::timeout, this, &AICallBase::onRequestTimeout);
}

AICallBase::~AICallBase()
{
    cancelRequest();
}

void AICallBase::setTimeout(int msec)
{
    if (msec > 0) {
        m_timeout = msec;
    }
}

bool AICallBase::isAllowedApiUrl(const QString &url)
{
    const QUrl parsed(url.trimmed());
    if (!parsed.isValid()) {
        return false;
    }
    const QString scheme = parsed.scheme().toLower();
    if (scheme == QLatin1String("https")) {
        return true;
    }
    if (scheme == QLatin1String("http")) {
        const QString host = parsed.host().toLower();
        return host == QLatin1String("localhost")
            || host == QLatin1String("127.0.0.1")
            || host == QLatin1String("::1");
    }
    return false;
}

QString AICallBase::lastErrorString() const
{
    return m_lastErrorString;
}

AIErrorType AICallBase::lastErrorType() const
{
    return m_lastErrorType;
}

void AICallBase::cancelRequest()
{
    // 停止超时定时器
    m_timeoutTimer->stop();

    // 取消当前请求：先本地保存并置空成员、断开信号，再 abort()。
    // 否则 abort() 会同步触发已连接的 finished/errorOccurred，重入
    // onReplyFinished() 把 m_currentReply 置空后，本函数随后的
    // m_currentReply->deleteLater() 会解引用空指针导致崩溃。
    QNetworkReply *reply = m_currentReply;
    m_currentReply = nullptr;
    if (reply) {
        disconnect(reply, nullptr, this, nullptr);
        reply->abort();
        reply->deleteLater();
    }

    // 重置错误信息
    m_lastErrorType = AIErrorType::NoError;
    m_lastErrorString.clear();
}

void AICallBase::setError(AIErrorType errorType, const QString &errorMsg)
{
    m_lastErrorType = errorType;
    m_lastErrorString = errorMsg;
    emit requestFailed(errorType, errorMsg);
}

void AICallBase::onReplyFinished(QNetworkReply *reply)
{
    // 停止超时定时器
    m_timeoutTimer->stop();

    // 清理Reply
    reply->deleteLater();
    m_currentReply = nullptr;

    if (reply->error() != QNetworkReply::NoError) {
        setError(AIErrorType::NetworkError, reply->errorString());
        return;
    }

    // 读取响应数据
    QByteArray responseData = reply->readAll();

    // 解析响应（子类实现）
    QString result;
    try {
        result = parseResponse(responseData);
    } catch (const std::exception &e) {
        setError(AIErrorType::ApiError, tr("响应解析失败：%1").arg(e.what()));
        return;
    } catch (...) {
        setError(AIErrorType::ApiError, tr("响应解析失败：未知错误"));
        return;
    }

    // 发送成功信号
    emit requestSuccess(result);
}

void AICallBase::onReplyError(QNetworkReply::NetworkError error)
{
    Q_UNUSED(error);
    // 错误处理在onReplyFinished中统一处理
}

void AICallBase::onRequestTimeout()
{
    // 与 cancelRequest() 同理：先断开信号再 abort，避免重入 onReplyFinished()
    QNetworkReply *reply = m_currentReply;
    m_currentReply = nullptr;
    if (reply) {
        disconnect(reply, nullptr, this, nullptr);
        reply->abort();
        reply->deleteLater();
    }

    setError(AIErrorType::TimeoutError, tr("请求超时（%1毫秒）").arg(m_timeout));
}
