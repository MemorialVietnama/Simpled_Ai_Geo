#include "scenemanager.h"

SceneManager::SceneManager(QStackedWidget *stackedWidget, QObject *parent)
    : QObject(parent)
    , stackedWidget(stackedWidget)
    , fileSelectionScene(nullptr)
    , loaderScene(nullptr)
    , analysisScene(nullptr)
    , simplifyScene(nullptr)
    , loaderTimer(nullptr)
{
    initialize();
}

SceneManager::~SceneManager()
{
    if (loaderTimer) {
        loaderTimer->stop();
    }
}

void SceneManager::initialize()
{
    // Create scenes
    fileSelectionScene = new FileSelectionScene();
    loaderScene = new LoaderScene();
    analysisScene = new AnalysisScene();
    simplifyScene = new SimplifyScene();

    // Add scenes to stacked widget
    stackedWidget->addWidget(fileSelectionScene);
    stackedWidget->addWidget(loaderScene);
    stackedWidget->addWidget(analysisScene);
    stackedWidget->addWidget(simplifyScene);

    // Setup connections
    setupConnections();

    // Initialize timer for loader
    loaderTimer = new QTimer(this);
    loaderTimer->setSingleShot(true);
    connect(loaderTimer, &QTimer::timeout, this, &SceneManager::goToAnalysis);

    // Start with file selection scene
    goToFileSelection();
}

void SceneManager::setupConnections()
{
    // File selection scene connections
    connect(fileSelectionScene, &FileSelectionScene::fileSelected, 
            this, &SceneManager::onFileSelected);
    connect(fileSelectionScene, &FileSelectionScene::analysisRequested, 
            this, &SceneManager::onAnalysisRequested);

    // Loader scene connections
    connect(loaderScene, &LoaderScene::animationFinished, 
            this, &SceneManager::goToAnalysis);

    // Analysis scene connections
    connect(analysisScene, &AnalysisScene::backRequested, 
            this, &SceneManager::onBackRequested);
    connect(analysisScene, &AnalysisScene::analysisFinished, 
            this, &SceneManager::onAnalysisFinished);
    connect(analysisScene, &AnalysisScene::simplifyRequested, 
            this, &SceneManager::onSimplifyRequested);
    
    // Simplify scene connections
    connect(simplifyScene, &SimplifyScene::backRequested, 
            this, &SceneManager::onSimplifyBackRequested);
    connect(simplifyScene, &SimplifyScene::simplificationFinished, 
            this, &SceneManager::onSimplificationFinished);
}

void SceneManager::goToFileSelection()
{
    switchToScene(fileSelectionScene, "FileSelection");
}

void SceneManager::goToLoader()
{
    switchToScene(loaderScene, "Loader");
    loaderScene->startAnimation();
    
    // Auto-transition to analysis after 2 seconds
    loaderTimer->start(2000);
}

void SceneManager::goToAnalysis()
{
    switchToScene(analysisScene, "Analysis");
    analysisScene->setModelPath(currentFilePath);
    analysisScene->startAnalysis();
}

void SceneManager::goToSimplify()
{
    switchToScene(simplifyScene, "Simplify");
    // TODO: Передать данные модели в сцену упрощения
}

void SceneManager::switchToScene(QWidget *scene, const QString &sceneName)
{
    if (stackedWidget && scene) {
        stackedWidget->setCurrentWidget(scene);
        emit sceneChanged(sceneName);
    }
}

void SceneManager::onFileSelected(const QString &filePath)
{
    currentFilePath = filePath;
}

void SceneManager::onAnalysisRequested()
{
    goToLoader();
}

void SceneManager::onBackRequested()
{
    goToFileSelection();
}

void SceneManager::onAnalysisFinished()
{
    // Analysis is complete, user can now use simplify functionality
    // This is handled by the analysis scene itself
}

void SceneManager::onSimplifyRequested()
{
    goToSimplify();
}

void SceneManager::onSimplifyBackRequested()
{
    goToAnalysis();
}

void SceneManager::onSimplificationFinished()
{
    // Можно добавить логику после завершения упрощения
    // Например, показать результаты или вернуться к анализу
    goToAnalysis();
}
