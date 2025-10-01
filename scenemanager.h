#ifndef SCENEMANAGER_H
#define SCENEMANAGER_H

#include <QObject>
#include <QStackedWidget>
#include <QTimer>
#include "fileselectionscene.h"
#include "loaderscene.h"
#include "analysisscene.h"
#include "simplifyscene.h"

class SceneManager : public QObject
{
    Q_OBJECT

public:
    explicit SceneManager(QStackedWidget *stackedWidget, QObject *parent = nullptr);
    ~SceneManager();

    void initialize();
    void goToFileSelection();
    void goToLoader();
    void goToAnalysis();
    void goToSimplify();

    FileSelectionScene* getFileSelectionScene() const { return fileSelectionScene; }
    LoaderScene* getLoaderScene() const { return loaderScene; }
    AnalysisScene* getAnalysisScene() const { return analysisScene; }
    SimplifyScene* getSimplifyScene() const { return simplifyScene; }

signals:
    void sceneChanged(const QString &sceneName);

private slots:
    void onFileSelected(const QString &filePath);
    void onAnalysisRequested();
    void onBackRequested();
    void onAnalysisFinished();
    void onSimplifyRequested();
    void onSimplifyBackRequested();
    void onSimplificationFinished();

private:
    void setupConnections();
    void switchToScene(QWidget *scene, const QString &sceneName);

    QStackedWidget *stackedWidget;
    FileSelectionScene *fileSelectionScene;
    LoaderScene *loaderScene;
    AnalysisScene *analysisScene;
    SimplifyScene *simplifyScene;
    
    QString currentFilePath;
    QTimer *loaderTimer;
};

#endif // SCENEMANAGER_H
