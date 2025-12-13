#include "scenemanager.h"
#include <QMetaObject>
#include <QApplication>
#include <QFile>
#include <QJsonDocument>
#include <QMessageBox>
#include <QFileInfo>
#include <QProcess>
#include <QDir>
#include <QTextStream>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QStringConverter>
#else
#include <QTextCodec>
#endif

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

    // Loader scene: больше не переключаемся по окончании анимации

    // Analysis scene connections
    connect(analysisScene, &AnalysisScene::backRequested, 
            this, &SceneManager::onBackRequested);
    connect(analysisScene, &AnalysisScene::analysisFinished, 
            this, &SceneManager::onAnalysisFinished);
    connect(analysisScene, &AnalysisScene::simplifyRequested, 
            this, &SceneManager::onSimplifyRequested);
    connect(analysisScene, &AnalysisScene::analysisLogReset,
            loaderScene, &LoaderScene::handleLogReset);
    connect(analysisScene, &AnalysisScene::analysisLogMessage,
            loaderScene, &LoaderScene::handleLogMessage);
    
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
    
    // Принудительно обновляем UI, чтобы сцена переключилась моментально
    QApplication::processEvents();
    
    // Запускаем анализ асинхронно, оставаясь на лоадере
    if (!currentFilePath.isEmpty()) {
        analysisScene->setModelPath(currentFilePath);
        // Передаем путь к обучающим данным, если он есть
        if (!currentTrainingDataPath.isEmpty()) {
            analysisScene->setTrainingDataPath(currentTrainingDataPath);
        }
        QMetaObject::invokeMethod(analysisScene, [this]() {
            analysisScene->startAnalysis();
        }, Qt::QueuedConnection);
    }
}

void SceneManager::goToAnalysis()
{
    // Просто переключаемся на сцену анализа (анализ уже запущен на лоадере)
    switchToScene(analysisScene, "Analysis");
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
    // Получаем путь к обучающим данным из fileSelectionScene
    if (fileSelectionScene) {
        currentTrainingDataPath = fileSelectionScene->getTrainingDataPath();
    }
    goToLoader();
}

void SceneManager::onBackRequested()
{
    goToFileSelection();
}

void SceneManager::onAnalysisFinished()
{
    // Анализ завершён: сохраняем данные и переходим к сцене анализа
    if (loaderScene) {
        loaderScene->stopAnimation();
    }
    if (analysisScene) {
        QJsonObject modelData = analysisScene->getModelData();
        if (!modelData.isEmpty()) {
            originalModelData = modelData;
        }
    }
    goToAnalysis();
}

void SceneManager::onSimplifyRequested()
{
    goToSimplify();
    
    // Передаем тренировочные данные из AnalysisScene в SimplifyScene
    if (analysisScene && simplifyScene) {
        QJsonObject trainingData = analysisScene->getTrainingData();
        if (!trainingData.isEmpty()) {
            simplifyScene->setTrainingData(trainingData);
            qDebug() << "[SceneManager] onSimplifyRequested: тренировочные данные переданы в SimplifyScene";
        } else {
            qDebug() << "[SceneManager] onSimplifyRequested: тренировочные данные отсутствуют";
        }
    }
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
    
    // Передаем данные полигонов из сцены упрощения
    if (simplifyScene) {
        comparisonScene->setSimplificationReport(simplifyScene->getSimplificationReport());
        QVector<LinearFunction> originalLines = simplifyScene->getOriginalLines();
        QVector<LinearFunction> simplifiedLines = simplifyScene->getSimplifiedLines();
        DecisionPolygon originalPolygon = simplifyScene->getOriginalPolygon();
        DecisionPolygon simplifiedPolygon = simplifyScene->getSimplifiedPolygon();
        
        comparisonScene->setPolygonData(originalLines, simplifiedLines, originalPolygon, simplifiedPolygon);
        
        // Передаем точки обучающих данных, если они есть в SimplifyScene
        QVector<QPointF> trainingPoints = simplifyScene->getTrainingDataPoints();
        if (!trainingPoints.isEmpty()) {
            comparisonScene->setTrainingDataPoints(trainingPoints);
            qDebug() << "  - Передано точек обучающих данных:" << trainingPoints.size();
        }
    }
    
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
    if (filePath.isEmpty()) {
        QMessageBox::warning(nullptr, "Ошибка", "Путь для сохранения не указан.");
        return;
    }

    if (simplifiedModelData.isEmpty()) {
        QMessageBox::warning(nullptr, "Ошибка", "Упрощенная модель отсутствует. Выполните упрощение перед сохранением.");
        return;
    }

    QFileInfo fileInfo(filePath);
    QString extension = fileInfo.suffix().toLower();
    
    // Определяем формат исходной модели
    QString framework = simplifiedModelData.value("framework").toString();
    QString originalModelPath = simplifiedModelData.value("model_path").toString();
    
    // Если расширение .json или не указано, сохраняем в JSON
    if (extension.isEmpty() || extension == "json") {
        saveAsJson(filePath);
    }
    // Если исходная модель была Keras и запрашивается сохранение в .h5/.keras
    else if ((extension == "h5" || extension == "keras") && 
             (framework.contains("Keras") || framework.contains("TensorFlow"))) {
        saveAsOriginalFormat(filePath, "keras", originalModelPath);
    }
    // Если исходная модель была PyTorch и запрашивается сохранение в .pth/.pt
    else if ((extension == "pth" || extension == "pt") && 
             framework.contains("PyTorch")) {
        saveAsOriginalFormat(filePath, "pytorch", originalModelPath);
    }
    // По умолчанию сохраняем в JSON
    else {
        QString jsonPath = filePath;
        if (!jsonPath.endsWith(".json", Qt::CaseInsensitive)) {
            jsonPath += ".json";
        }
        saveAsJson(jsonPath);
        QMessageBox::information(nullptr, "Информация", 
            QString("Модель сохранена в формате JSON: %1\n\n"
                    "Для сохранения в исходном формате (%2) используйте расширение .%3")
            .arg(jsonPath)
            .arg(framework)
            .arg(framework.contains("Keras") ? "h5" : "pth"));
    }
}

void SceneManager::saveAsJson(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(nullptr, "Ошибка", 
            QString("Не удалось открыть файл для записи:\n%1\n\n%2")
            .arg(filePath)
            .arg(file.errorString()));
        return;
    }

    QJsonDocument doc(simplifiedModelData);
    QTextStream out(&file);
    #if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    out.setEncoding(QStringConverter::Utf8);
    #else
    out.setCodec("UTF-8");
    #endif
    out << doc.toJson(QJsonDocument::Indented);
    file.close();

    QMessageBox::information(nullptr, "Успех", 
        QString("Упрощенная модель успешно сохранена:\n%1")
        .arg(filePath));
    
    qDebug() << "Модель сохранена в JSON:" << filePath;
}

void SceneManager::saveAsOriginalFormat(const QString &filePath, const QString &format, const QString &originalModelPath)
{
    // Создаем Python скрипт для сохранения модели в исходном формате
    QString scriptPath = QDir::temp().absoluteFilePath("save_simplified_model.py");
    QFile scriptFile(scriptPath);
    
    if (!scriptFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(nullptr, "Ошибка", 
            QString("Не удалось создать временный скрипт:\n%1").arg(scriptFile.errorString()));
        return;
    }

    QTextStream scriptOut(&scriptFile);
    #if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    scriptOut.setEncoding(QStringConverter::Utf8);
    #else
    scriptOut.setCodec("UTF-8");
    #endif
    
    if (format == "keras") {
        scriptOut << R"(
import sys
import json
import tensorflow as tf
from tensorflow import keras
import numpy as np

def save_simplified_keras_model(json_path, original_model_path, output_path):
    try:
        # Загружаем упрощенную модель из JSON
        with open(json_path, 'r', encoding='utf-8') as f:
            simplified_data = json.load(f)
        
        # Загружаем оригинальную модель
        original_model = keras.models.load_model(original_model_path)
        
        # Создаем новую модель с упрощенной архитектурой
        layers_info = simplified_data.get('layers', [])
        if not layers_info:
            print("Ошибка: информация о слоях отсутствует в JSON", file=sys.stderr)
            return False
        
        # Строим упрощенную модель
        inputs = keras.Input(shape=original_model.input_shape[1:])
        x = inputs
        
        for i, layer_info in enumerate(layers_info):
            layer_type = layer_info.get('type', '')
            neurons = layer_info.get('neurons', 0)
            activation = layer_info.get('activation', 'relu')
            
            if 'Dense' in layer_type or 'dense' in layer_type.lower():
                x = keras.layers.Dense(neurons, activation=activation)(x)
            elif 'Conv2D' in layer_type or 'conv2d' in layer_type.lower():
                filters = neurons
                x = keras.layers.Conv2D(filters, (3, 3), activation=activation, padding='same')(x)
            # Добавьте другие типы слоев по необходимости
        
        # Создаем выходной слой (берем из оригинальной модели)
        output_layer = original_model.layers[-1]
        if hasattr(output_layer, 'units'):
            outputs = keras.layers.Dense(output_layer.units, activation=output_layer.activation)(x)
        else:
            outputs = x
        
        simplified_model = keras.Model(inputs=inputs, outputs=outputs)
        
        # Копируем веса из оригинальной модели где возможно
        try:
            for orig_layer, new_layer in zip(original_model.layers[:-1], simplified_model.layers[:-1]):
                if orig_layer.get_weights() and len(orig_layer.get_weights()) == len(new_layer.get_weights()):
                    # Обрезаем веса до нужного размера
                    orig_weights = orig_layer.get_weights()
                    new_weights = []
                    for orig_w, new_w in zip(orig_weights, new_layer.get_weights()):
                        if orig_w.shape == new_w.shape:
                            new_weights.append(orig_w)
                        else:
                            # Обрезаем или дополняем веса
                            if len(orig_w.shape) == 2:
                                new_weights.append(orig_w[:new_w.shape[0], :new_w.shape[1]])
                            else:
                                new_weights.append(orig_w[:new_w.shape[0]])
                    new_layer.set_weights(new_weights)
        except Exception as e:
            print(f"Предупреждение: не удалось скопировать веса: {e}", file=sys.stderr)
        
        # Сохраняем упрощенную модель
        simplified_model.save(output_path)
        print(f"Упрощенная модель сохранена: {output_path}", file=sys.stderr)
        return True
        
    except Exception as e:
        print(f"Ошибка при сохранении модели: {e}", file=sys.stderr)
        import traceback
        traceback.print_exc()
        return False

if __name__ == "__main__":
    if len(sys.argv) != 4:
        print("Использование: python save_simplified_model.py <json_path> <original_model_path> <output_path>", file=sys.stderr)
        sys.exit(1)
    
    json_path = sys.argv[1]
    original_model_path = sys.argv[2]
    output_path = sys.argv[3]
    
    success = save_simplified_keras_model(json_path, original_model_path, output_path)
    sys.exit(0 if success else 1)
)";
    } else if (format == "pytorch") {
        scriptOut << R"(
import sys
import json
import torch
import torch.nn as nn

def save_simplified_pytorch_model(json_path, original_model_path, output_path):
    try:
        # Загружаем упрощенную модель из JSON
        with open(json_path, 'r', encoding='utf-8') as f:
            simplified_data = json.load(f)
        
        # Загружаем оригинальную модель
        checkpoint = torch.load(original_model_path, map_location='cpu')
        if isinstance(checkpoint, dict) and 'model' in checkpoint:
            original_model = checkpoint['model']
        else:
            original_model = checkpoint
        
        # Создаем упрощенную модель на основе JSON
        layers_info = simplified_data.get('layers', [])
        if not layers_info:
            print("Ошибка: информация о слоях отсутствует в JSON", file=sys.stderr)
            return False
        
        # Строим упрощенную модель
        class SimplifiedModel(nn.Module):
            def __init__(self, layers_info):
                super(SimplifiedModel, self).__init__()
                self.layers = nn.ModuleList()
                
                for layer_info in layers_info:
                    layer_type = layer_info.get('type', '')
                    neurons = layer_info.get('neurons', 0)
                    activation = layer_info.get('activation', 'relu')
                    
                    if 'Linear' in layer_type or 'linear' in layer_type.lower():
                        # Нужно знать размер входа для первого слоя
                        if len(self.layers) == 0:
                            # Берем из оригинальной модели
                            if hasattr(original_model, 'layers') and len(original_model.layers) > 0:
                                first_layer = original_model.layers[0]
                                if hasattr(first_layer, 'in_features'):
                                    in_features = first_layer.in_features
                                else:
                                    in_features = neurons  # Fallback
                            else:
                                in_features = neurons
                        else:
                            in_features = self.layers[-1].out_features
                        
                        linear = nn.Linear(in_features, neurons)
                        self.layers.append(linear)
                        
                        # Добавляем активацию
                        if activation == 'relu':
                            self.layers.append(nn.ReLU())
                        elif activation == 'sigmoid':
                            self.layers.append(nn.Sigmoid())
                        elif activation == 'tanh':
                            self.layers.append(nn.Tanh())
            
            def forward(self, x):
                for layer in self.layers:
                    x = layer(x)
                return x
        
        simplified_model = SimplifiedModel(layers_info)
        
        # Пытаемся скопировать веса из оригинальной модели
        try:
            if hasattr(original_model, 'state_dict'):
                orig_state = original_model.state_dict()
                new_state = simplified_model.state_dict()
                
                for key in new_state.keys():
                    if key in orig_state:
                        orig_weight = orig_state[key]
                        new_weight = new_state[key]
                        if orig_weight.shape == new_weight.shape:
                            new_state[key] = orig_weight
                        else:
                            # Обрезаем веса
                            if len(orig_weight.shape) == 2:
                                new_state[key] = orig_weight[:new_weight.shape[0], :new_weight.shape[1]]
                            else:
                                new_state[key] = orig_weight[:new_weight.shape[0]]
                
                simplified_model.load_state_dict(new_state)
        except Exception as e:
            print(f"Предупреждение: не удалось скопировать веса: {e}", file=sys.stderr)
        
        # Сохраняем упрощенную модель
        torch.save(simplified_model.state_dict(), output_path)
        print(f"Упрощенная модель сохранена: {output_path}", file=sys.stderr)
        return True
        
    except Exception as e:
        print(f"Ошибка при сохранении модели: {e}", file=sys.stderr)
        import traceback
        traceback.print_exc()
        return False

if __name__ == "__main__":
    if len(sys.argv) != 4:
        print("Использование: python save_simplified_model.py <json_path> <original_model_path> <output_path>", file=sys.stderr)
        sys.exit(1)
    
    json_path = sys.argv[1]
    original_model_path = sys.argv[2]
    output_path = sys.argv[3]
    
    success = save_simplified_pytorch_model(json_path, original_model_path, output_path)
    sys.exit(0 if success else 1)
)";
    }
    
    scriptFile.close();
    
    // Сохраняем временный JSON файл
    QString tempJsonPath = QDir::temp().absoluteFilePath("simplified_model_temp.json");
    saveAsJson(tempJsonPath);
    
    // Запускаем Python скрипт
    QProcess *process = new QProcess(this);
    process->setProcessChannelMode(QProcess::MergedChannels);
    
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            [this, process, filePath, scriptPath, tempJsonPath](int exitCode, QProcess::ExitStatus) {
        if (exitCode == 0) {
            QMessageBox::information(nullptr, "Успех", 
                QString("Упрощенная модель успешно сохранена в исходном формате:\n%1")
                .arg(filePath));
        } else {
            QMessageBox::warning(nullptr, "Предупреждение", 
                QString("Не удалось сохранить модель в исходном формате.\n"
                        "Модель сохранена в JSON формате.\n\n"
                        "Проверьте логи для деталей."));
        }
        
        // Удаляем временные файлы
        QFile::remove(scriptPath);
        QFile::remove(tempJsonPath);
        process->deleteLater();
    });
    
    QString pythonCommand = "python";
    #ifdef Q_OS_WIN
    pythonCommand = "python";
    #endif
    
    QStringList arguments;
    arguments << scriptPath << tempJsonPath << originalModelPath << filePath;
    
    qDebug() << "Запуск Python скрипта для сохранения модели:" << pythonCommand << arguments;
    process->start(pythonCommand, arguments);
    
    if (!process->waitForStarted(5000)) {
        QMessageBox::critical(nullptr, "Ошибка", 
            QString("Не удалось запустить Python скрипт.\n"
                    "Убедитесь, что Python установлен и доступен в PATH.\n\n"
                    "Модель будет сохранена в JSON формате."));
        saveAsJson(filePath + ".json");
        QFile::remove(scriptPath);
        QFile::remove(tempJsonPath);
        process->deleteLater();
    }
}
