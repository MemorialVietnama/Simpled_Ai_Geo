#ifndef SCENEMANAGER_H
#define SCENEMANAGER_H

#include <QObject>
#include <QStackedWidget>
#include <QTimer>
#include "fileselectionscene.h"
#include "loaderscene.h"
#include "analysisscene.h"
#include "simplifyscene.h"
#include "comparisonscene.h"

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
    void goToComparison();

    FileSelectionScene* getFileSelectionScene() const { return fileSelectionScene; }
    LoaderScene* getLoaderScene() const { return loaderScene; }
    AnalysisScene* getAnalysisScene() const { return analysisScene; }
    SimplifyScene* getSimplifyScene() const { return simplifyScene; }
    ComparisonScene* getComparisonScene() const { return comparisonScene; }

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
    void onComparisonRequested(const QJsonObject &originalModel, const QJsonObject &simplifiedModel, const QJsonObject &result);
    void onComparisonBackRequested();
    void onSaveRequested(const QString &filePath);

private:
    void setupConnections();
    void switchToScene(QWidget *scene, const QString &sceneName);

    QStackedWidget *stackedWidget;
    FileSelectionScene *fileSelectionScene;
    LoaderScene *loaderScene;
    AnalysisScene *analysisScene;
    SimplifyScene *simplifyScene;
    ComparisonScene *comparisonScene;
    
    QString currentFilePath;
    QTimer *loaderTimer;
    
    // Данные для сравнения
    QJsonObject originalModelData;
    QJsonObject simplifiedModelData;
    QJsonObject simplificationResult;
};

#endif // SCENEMANAGER_H
