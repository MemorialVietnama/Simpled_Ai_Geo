#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QCloseEvent>
#include "scenemanager.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onSceneChanged(const QString &sceneName);

private:
    void setupUI();
    void applyStyles();

    QStackedWidget *stackedWidget;
    SceneManager *sceneManager;
};

#endif // MAINWINDOW_H
