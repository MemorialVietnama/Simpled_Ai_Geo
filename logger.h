#ifndef LOGGER_H
#define LOGGER_H

#include <QObject>
#include <QString>
#include <QTextEdit>
#include <QFile>
#include <QTextStream>
#include <memory>

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error,
    Critical
};

class Logger : public QObject
{
    Q_OBJECT

public:
    static Logger& getInstance();
    
    void setLogWidget(QTextEdit* widget);
    void setLogFile(const QString& filePath);
    void log(LogLevel level, const QString& message, const QString& context = "");
    void debug(const QString& message, const QString& context = "");
    void info(const QString& message, const QString& context = "");
    void warning(const QString& message, const QString& context = "");
    void error(const QString& message, const QString& context = "");
    void critical(const QString& message, const QString& context = "");

private:
    Logger() = default;
    ~Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    QString formatMessage(LogLevel level, const QString& message, const QString& context);
    QString getLevelString(LogLevel level);
    
    QTextEdit* m_logWidget = nullptr;
    std::unique_ptr<QFile> m_logFile;
    std::unique_ptr<QTextStream> m_logStream;
};

#define LOG_DEBUG(msg, ctx) Logger::getInstance().debug(msg, ctx)
#define LOG_INFO(msg, ctx) Logger::getInstance().info(msg, ctx)
#define LOG_WARNING(msg, ctx) Logger::getInstance().warning(msg, ctx)
#define LOG_ERROR(msg, ctx) Logger::getInstance().error(msg, ctx)
#define LOG_CRITICAL(msg, ctx) Logger::getInstance().critical(msg, ctx)

#endif // LOGGER_H
