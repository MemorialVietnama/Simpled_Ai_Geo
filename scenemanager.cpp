#include "scenemanager.h"

SceneManager::SceneManager(QStackedWidget *stackedWidget, QObject *parent)
    : QObject(parent)
    , stackedWidget(stackedWidget)
    , fileSelectionScene(nullptr)
    , loaderScene(nullptr)
    , analysisScene(nullptr)
    , simplifyScene(nullptr)
    , comparisonScene(nullptr)
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
    comparisonScene = new ComparisonScene();

    // Add scenes to stacked widget
    stackedWidget->addWidget(fileSelectionScene);
    stackedWidget->addWidget(loaderScene);
    stackedWidget->addWidget(analysisScene);
    stackedWidget->addWidget(simplifyScene);
    stackedWidget->addWidget(comparisonScene);

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
    connect(simplifyScene, &SimplifyScene::comparisonRequested,
            this, &SceneManager::onComparisonRequested);
    
    // Comparison scene connections
    connect(comparisonScene, &ComparisonScene::backRequested,
            this, &SceneManager::onComparisonBackRequested);
    connect(comparisonScene, &ComparisonScene::saveRequested,
            this, &SceneManager::onSaveRequested);
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
    
    // Передаем данные модели в сцену упрощения
    if (originalModelData.isEmpty()) {
        // Если нет данных, создаем тестовые данные
        QJsonObject testModel = QJsonObject{
            {"name", "Test Model"},
            {"size", 100.0},
            {"parameters", 1000},
            {"layers", QJsonArray{
                QJsonObject{
                    {"name", "Input Layer"},
                    {"neurons", QJsonArray{
                        QJsonObject{{"weight", 1.0}, {"activity", 0.8}},
                        QJsonObject{{"weight", 0.9}, {"activity", 0.7}},
                        QJsonObject{{"weight", 0.8}, {"activity", 0.6}}
                    }}
                },
                QJsonObject{
                    {"name", "Hidden Layer 1"},
                    {"neurons", QJsonArray{
                        QJsonObject{{"weight", 0.7}, {"activity", 0.5}},
                        QJsonObject{{"weight", 0.6}, {"activity", 0.4}}
                    }}
                },
                QJsonObject{
                    {"name", "Output Layer"},
                    {"neurons", QJsonArray{
                        QJsonObject{{"weight", 0.5}, {"activity", 0.3}}
                    }}
                }
            }}
        };
        originalModelData = testModel;
    }
    
    simplifyScene->setModelData(originalModelData);
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
    // Analysis is complete, получаем данные из сцены анализа
    if (analysisScene) {
        // Получаем данные модели из сцены анализа
        QJsonObject modelData = analysisScene->getModelData();
        if (!modelData.isEmpty()) {
            originalModelData = modelData;
        }
    }
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

void SceneManager::goToComparison()
{
    qDebug() << "SceneManager::goToComparison - переключение на сцену сравнения";
    qDebug() << "  - comparisonScene:" << (comparisonScene != nullptr);
    qDebug() << "  - originalModelData пуста:" << originalModelData.isEmpty();
    qDebug() << "  - simplifiedModelData пуста:" << simplifiedModelData.isEmpty();
    
    switchToScene(comparisonScene, "Comparison");
    comparisonScene->setModels(originalModelData, simplifiedModelData, simplificationResult);
    
    qDebug() << "  - Сцена сравнения активирована и данные переданы";
}

void SceneManager::onComparisonRequested(const QJsonObject &originalModel, const QJsonObject &simplifiedModel, const QJsonObject &result)
{
    qDebug() << "SceneManager::onComparisonRequested - переход к сравнению:";
    qDebug() << "  - Оригинальная модель пуста:" << originalModel.isEmpty();
    qDebug() << "  - Упрощенная модель пуста:" << simplifiedModel.isEmpty();
    qDebug() << "  - Результат пуст:" << result.isEmpty();
    
    originalModelData = originalModel;
    simplifiedModelData = simplifiedModel;
    simplificationResult = result;
    
    qDebug() << "  - Переход к сцене сравнения...";
    goToComparison();
}

void SceneManager::onComparisonBackRequested()
{
    goToSimplify();
}

void SceneManager::onSaveRequested(const QString &filePath)
{
    // Здесь можно добавить логику сохранения модели
    // Пока что просто выводим сообщение
    qDebug() << "Сохранение модели в:" << filePath;
    // TODO: Реализовать сохранение упрощенной модели
}
