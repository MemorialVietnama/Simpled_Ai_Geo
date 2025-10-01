#ifndef LOADERSCENE_H
#define LOADERSCENE_H

#include <QWidget>
#include <QLabel>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QTimer>

class LoaderScene : public QWidget
{
    Q_OBJECT

public:
    explicit LoaderScene(QWidget *parent = nullptr);
    ~LoaderScene();

    void startAnimation();
    void stopAnimation();

signals:
    void animationFinished();

private slots:
    void updateStatusText();

private:
    void setupUI();

    // UI Elements
    QLabel *brainIcon;
    QLabel *loadingText;
    QLabel *statusText;
    QProgressBar *progressBar;
    
    // Animation
    QTimer *statusTimer;
    int statusIndex;
    static const QStringList statusMessages;
};

#endif // LOADERSCENE_H
