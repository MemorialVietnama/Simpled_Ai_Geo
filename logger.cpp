#include "logger.h"
#include <QDateTime>
#include <QDir>
#include <QStandardPaths>
#include <QDebug>

Logger& Logger::getInstance()
{
    static Logger instance;
    return instance;
}

void Logger::setLogWidget(QTextEdit* widget)
{
    m_logWidget = widget;
}

void Logger::setLogFile(const QString& filePath)
{
    try {
        m_logFile = std::make_unique<QFile>(filePath);
        if (m_logFile->open(QIODevice::WriteOnly | QIODevice::Append)) {
            m_logStream = std::make_unique<QTextStream>(m_logFile.get());
        }
    } catch (const std::exception& e) {
        qDebug() << "Failed to open log file:" << e.what();
    }
}

void Logger::log(LogLevel level, const QString& message, const QString& context)
{
    QString formattedMessage = formatMessage(level, message, context);
    
    // Log to widget if available
    if (m_logWidget) {
        m_logWidget->append(formattedMessage);
        // Auto-scroll to bottom
        QTextCursor cursor = m_logWidget->textCursor();
        cursor.movePosition(QTextCursor::End);
        m_logWidget->setTextCursor(cursor);
    }
    
    // Log to file if available
    if (m_logStream) {
        *m_logStream << formattedMessage << Qt::endl;
        m_logStream->flush();
    }
    
    // Also log to Qt debug output
    qDebug() << formattedMessage;
}

void Logger::debug(const QString& message, const QString& context)
{
    log(LogLevel::Debug, message, context);
}

void Logger::info(const QString& message, const QString& context)
{
    log(LogLevel::Info, message, context);
}

void Logger::warning(const QString& message, const QString& context)
{
    log(LogLevel::Warning, message, context);
}

void Logger::error(const QString& message, const QString& context)
{
    log(LogLevel::Error, message, context);
}

void Logger::critical(const QString& message, const QString& context)
{
    log(LogLevel::Critical, message, context);
}

QString Logger::formatMessage(LogLevel level, const QString& message, const QString& context)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    QString levelStr = getLevelString(level);
    QString contextStr = context.isEmpty() ? "" : " [" + context + "]";
    
    return QString("[%1] %2%3: %4")
        .arg(timestamp)
        .arg(levelStr)
        .arg(contextStr)
        .arg(message);
}

QString Logger::getLevelString(LogLevel level)
{
    switch (level) {
        case LogLevel::Debug:    return "DEBUG";
        case LogLevel::Info:     return "INFO ";
        case LogLevel::Warning:  return "WARN ";
        case LogLevel::Error:    return "ERROR";
        case LogLevel::Critical: return "CRIT ";
        default:                 return "UNK  ";
    }
}
