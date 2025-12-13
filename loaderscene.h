#ifndef LOADERSCENE_H
#define LOADERSCENE_H

#include <QWidget>
#include <QLabel>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QStringList>

class LoaderScene : public QWidget
{
    Q_OBJECT

public:
    explicit LoaderScene(QWidget *parent = nullptr);
    ~LoaderScene();

    void startAnimation();
    void stopAnimation();
    void resetStatus();
    void appendStatusMessage(const QString &message);

public slots:
    void handleLogReset();
    void handleLogMessage(const QString &message);

private:
    void setupUI();
    void updateStatusLabel();

    // UI Elements
    QLabel *brainIcon;
    QLabel *loadingText;
    QLabel *statusText;
    QProgressBar *progressBar;
    QStringList statusLog;
    int maxLogLines = 6;
};

#endif // LOADERSCENE_H
