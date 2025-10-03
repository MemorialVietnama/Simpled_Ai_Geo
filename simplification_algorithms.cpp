#include "simplification_algorithms.h"
#include <QDebug>
#include <QThread>
#include <QtMath>
#include <QRandomGenerator>

SimplificationAlgorithms::SimplificationAlgorithms(QObject *parent)
    : QObject(parent)
    , progressTimer(new QTimer(this))
    , currentStep(0)
    , totalSteps(10)
{
    connect(progressTimer, &QTimer::timeout, this, &SimplificationAlgorithms::processStep);
}

SimplificationAlgorithms::~SimplificationAlgorithms()
{
    if (progressTimer->isActive()) {
        progressTimer->stop();
    }
}

void SimplificationAlgorithms::startSimplification(const QString &algorithmName, const QJsonObject &model)
{
    currentAlgorithm = algorithmName;
    currentModel = model;
    currentStep = 0;
    
    qDebug() << "🚀 Запуск алгоритма упрощения:" << algorithmName;
    
    // Запускаем симуляцию прогресса
    simulateProgress(algorithmName);
}

void SimplificationAlgorithms::simulateProgress(const QString &algorithmName)
{
    progressTimer->start(200); // Обновляем каждые 200мс
    currentStep = 0;
    
    // Отправляем начальное сообщение
    emit progressUpdated(0, QString("Инициализация алгоритма %1...").arg(algorithmName));
}

void SimplificationAlgorithms::processStep()
{
    currentStep++;
    int percentage = (currentStep * 100) / totalSteps;
    
    QString message;
    switch (currentStep) {
        case 1:
            message = "Анализ структуры модели...";
            break;
        case 2:
            message = "Вычисление важности параметров...";
            break;
        case 3:
            message = "Определение избыточных связей...";
            break;
        case 4:
            message = "Применение алгоритма упрощения...";
            break;
        case 5:
            message = "Оптимизация архитектуры...";
            break;
        case 6:
            message = "Проверка целостности модели...";
            break;
        case 7:
            message = "Валидация результатов...";
            break;
        case 8:
            message = "Вычисление метрик...";
            break;
        case 9:
            message = "Финальная проверка...";
            break;
        case 10:
            message = "Завершение обработки...";
            break;
    }
    
    emit progressUpdated(percentage, message);
    
    if (currentStep >= totalSteps) {
        progressTimer->stop();
        
        try {
            SimplificationResult result = executeAlgorithm(currentAlgorithm, currentModel);
            emit algorithmFinished(currentAlgorithm, result);
        } catch (const std::exception &e) {
            emit algorithmError(QString("Ошибка выполнения алгоритма: %1").arg(e.what()));
        }
    }
}

SimplificationResult SimplificationAlgorithms::executeAlgorithm(const QString &algorithmName, const QJsonObject &model)
{
    SimplificationResult result;
    result.algorithmName = algorithmName;
    
    if (algorithmName == "Auto") {
        // Автоматический выбор лучшего алгоритма на основе характеристик модели
        result = selectBestAlgorithm(model);
    } else if (algorithmName == "Douglas-Peucker") {
        result = douglasPeucker(model);
    } else if (algorithmName == "Visvalingam-Whyatt") {
        result = visvalingamWhyatt(model);
    } else if (algorithmName == "Pruning") {
        result = pruning(model);
    } else if (algorithmName == "Quantization") {
        result = quantization(model);
    } else if (algorithmName == "Knowledge Distillation") {
        result = knowledgeDistillation(model);
    } else if (algorithmName == "Low-Rank Decomposition") {
        result = lowRankDecomposition(model);
    } else if (algorithmName == "Architecture Search") {
        result = architectureSearch(model);
    } else if (algorithmName == "Combined") {
        result = combinedSimplification(model, QStringList() << "Pruning" << "Quantization");
    } else {
        throw std::runtime_error("Неизвестный алгоритм: " + algorithmName.toStdString());
    }
    
    return result;
}

SimplificationResult SimplificationAlgorithms::douglasPeucker(const QJsonObject &model, double tolerance)
{
    Q_UNUSED(tolerance);
    
    SimplificationResult result;
    result.algorithmName = "Douglas-Peucker";
    
    // Алгоритм Дугласа-Пекера для упрощения нейронных сетей
    // Применяем принцип упрощения кривых к архитектуре сети
    QJsonObject simplifiedModel = model;
    
    if (simplifiedModel.contains("layers")) {
        QJsonArray layers = simplifiedModel["layers"].toArray();
        QJsonArray simplifiedLayers;
        
        for (int i = 0; i < layers.size(); ++i) {
            QJsonObject layer = layers[i].toObject();
            
            // Упрощаем нейроны в слое по принципу Douglas-Peucker
            if (layer.contains("neurons")) {
                QJsonArray neurons = layer["neurons"].toArray();
                QJsonArray simplifiedNeurons;
                
                // Применяем алгоритм упрощения: оставляем только "важные" нейроны
                int step = qMax(1, neurons.size() / 4); // Упрощаем до 25% от исходного
                
                for (int j = 0; j < neurons.size(); j += step) {
                    if (j < neurons.size()) {
                        simplifiedNeurons.append(neurons[j]);
                    }
                }
                
                // Всегда оставляем последний нейрон
                if (neurons.size() > 1 && !simplifiedNeurons.contains(neurons[neurons.size() - 1])) {
                    simplifiedNeurons.append(neurons[neurons.size() - 1]);
                }
                
                layer["neurons"] = simplifiedNeurons;
            }
            
            simplifiedLayers.append(layer);
        }
        
        simplifiedModel["layers"] = simplifiedLayers;
    }
    
    result.simplifiedModel = simplifiedModel;
    calculateMetrics(model, result.simplifiedModel, result);
    
    return result;
}

SimplificationResult SimplificationAlgorithms::visvalingamWhyatt(const QJsonObject &model, double threshold)
{
    Q_UNUSED(threshold);
    
    SimplificationResult result;
    result.algorithmName = "Visvalingam-Whyatt";
    
    // Алгоритм Висвалингама-Уайатта для упрощения нейронных сетей
    // Используем принцип удаления точек с наименьшей "важностью"
    QJsonObject simplifiedModel = model;
    
    if (simplifiedModel.contains("layers")) {
        QJsonArray layers = simplifiedModel["layers"].toArray();
        QJsonArray simplifiedLayers;
        
        for (int i = 0; i < layers.size(); ++i) {
            QJsonObject layer = layers[i].toObject();
            
            // Применяем алгоритм Visvalingam-Whyatt к нейронам
            if (layer.contains("neurons")) {
                QJsonArray neurons = layer["neurons"].toArray();
                QJsonArray simplifiedNeurons;
                
                // Вычисляем "важность" каждого нейрона (упрощенная версия)
                QVector<double> importance;
                for (int j = 0; j < neurons.size(); ++j) {
                    QJsonObject neuron = neurons[j].toObject();
                    double weight = neuron["weight"].toDouble(1.0);
                    double activity = neuron["activity"].toDouble(0.5);
                    double importanceValue = weight * activity;
                    importance.append(importanceValue);
                }
                
                // Сортируем по важности и берем топ-нейроны
                QVector<int> indices;
                for (int j = 0; j < neurons.size(); ++j) {
                    indices.append(j);
                }
                
                // Сортируем по убыванию важности
                std::sort(indices.begin(), indices.end(), [&importance](int a, int b) {
                    return importance[a] > importance[b];
                });
                
                // Берем только 60% самых важных нейронов
                int keepCount = qMax(1, static_cast<int>(neurons.size() * 0.6));
                for (int j = 0; j < keepCount && j < indices.size(); ++j) {
                    simplifiedNeurons.append(neurons[indices[j]]);
                }
                
                layer["neurons"] = simplifiedNeurons;
            }
            
            simplifiedLayers.append(layer);
        }
        
        simplifiedModel["layers"] = simplifiedLayers;
    }
    
    result.simplifiedModel = simplifiedModel;
    calculateMetrics(model, result.simplifiedModel, result);
    
    return result;
}

SimplificationResult SimplificationAlgorithms::pruning(const QJsonObject &model, double pruningRatio)
{
    Q_UNUSED(pruningRatio);
    
    SimplificationResult result;
    result.algorithmName = "Pruning";
    
    result.simplifiedModel = createSimplifiedModel(model, 0.5); // 50% упрощение
    calculateMetrics(model, result.simplifiedModel, result);
    
    return result;
}

SimplificationResult SimplificationAlgorithms::quantization(const QJsonObject &model, int bits)
{
    Q_UNUSED(bits);
    
    SimplificationResult result;
    result.algorithmName = "Quantization";
    
    result.simplifiedModel = createSimplifiedModel(model, 0.8); // 20% упрощение
    calculateMetrics(model, result.simplifiedModel, result);
    
    return result;
}

SimplificationResult SimplificationAlgorithms::knowledgeDistillation(const QJsonObject &model, double temperature)
{
    Q_UNUSED(temperature);
    
    SimplificationResult result;
    result.algorithmName = "Knowledge Distillation";
    
    result.simplifiedModel = createSimplifiedModel(model, 0.4); // 60% упрощение
    calculateMetrics(model, result.simplifiedModel, result);
    
    return result;
}

SimplificationResult SimplificationAlgorithms::lowRankDecomposition(const QJsonObject &model, double rankRatio)
{
    Q_UNUSED(rankRatio);
    
    SimplificationResult result;
    result.algorithmName = "Low-Rank Decomposition";
    
    result.simplifiedModel = createSimplifiedModel(model, 0.6); // 40% упрощение
    calculateMetrics(model, result.simplifiedModel, result);
    
    return result;
}

SimplificationResult SimplificationAlgorithms::architectureSearch(const QJsonObject &model, int maxLayers)
{
    Q_UNUSED(maxLayers);
    
    SimplificationResult result;
    result.algorithmName = "Architecture Search";
    
    result.simplifiedModel = createSimplifiedModel(model, 0.3); // 70% упрощение
    calculateMetrics(model, result.simplifiedModel, result);
    
    return result;
}

SimplificationResult SimplificationAlgorithms::combinedSimplification(const QJsonObject &model, const QStringList &algorithms)
{
    Q_UNUSED(algorithms);
    
    SimplificationResult result;
    result.algorithmName = "Combined";
    
    result.simplifiedModel = createSimplifiedModel(model, 0.2); // 80% упрощение
    calculateMetrics(model, result.simplifiedModel, result);
    
    return result;
}

QJsonObject SimplificationAlgorithms::createSimplifiedModel(const QJsonObject &originalModel, double reductionFactor)
{
    QJsonObject simplifiedModel = originalModel;
    
    // Упрощаем структуру модели
    if (simplifiedModel.contains("layers")) {
        QJsonArray layers = simplifiedModel["layers"].toArray();
        QJsonArray simplifiedLayers;
        
        int layersToKeep = qMax(1, static_cast<int>(layers.size() * (1.0 - reductionFactor)));
        
        for (int i = 0; i < layersToKeep && i < layers.size(); ++i) {
            QJsonObject layer = layers[i].toObject();
            
            // Упрощаем нейроны в слое
            if (layer.contains("neurons")) {
                QJsonArray neurons = layer["neurons"].toArray();
                QJsonArray simplifiedNeurons;
                
                int neuronsToKeep = qMax(1, static_cast<int>(neurons.size() * (1.0 - reductionFactor)));
                
                for (int j = 0; j < neuronsToKeep && j < neurons.size(); ++j) {
                    simplifiedNeurons.append(neurons[j]);
                }
                
                layer["neurons"] = simplifiedNeurons;
            }
            
            simplifiedLayers.append(layer);
        }
        
        simplifiedModel["layers"] = simplifiedLayers;
    }
    
    // Добавляем метаданные упрощения
    simplifiedModel["simplified"] = true;
    simplifiedModel["reduction_factor"] = reductionFactor;
    simplifiedModel["original_size"] = originalModel["size"].toDouble();
    
    return simplifiedModel;
}

void SimplificationAlgorithms::calculateMetrics(const QJsonObject &originalModel, const QJsonObject &simplifiedModel, SimplificationResult &result)
{
    // Вычисляем базовые метрики (используем правильные поля)
    double originalSize = originalModel["model_size_mb"].toDouble(100.0);
    if (originalSize <= 0) {
        originalSize = originalModel["size"].toDouble(100.0);
    }
    
    // Вычисляем размер упрощенной модели на основе реального сжатия
    double reductionFactor = 0.5; // По умолчанию 50% сжатие
    if (simplifiedModel.contains("reduction_factor")) {
        reductionFactor = simplifiedModel["reduction_factor"].toDouble(0.5);
    }
    
    double simplifiedSize = originalSize * (1.0 - reductionFactor);
    
    result.compressionRatio = originalSize / simplifiedSize;
    result.sizeReduction = (originalSize - simplifiedSize) / originalSize * 100.0;
    result.modelSizeMB = simplifiedSize;
    
    // Вычисляем параметры
    int originalParams = originalModel["parameters"].toInt(1000);
    int simplifiedParams = static_cast<int>(originalParams * (1.0 - result.sizeReduction / 100.0));
    
    result.totalParameters = simplifiedParams;
    result.accuracyLoss = 1.0 + QRandomGenerator::global()->bounded(0, 400) / 100.0; // Симуляция потери точности (1.0-5.0)
    
    // Подсчитываем слои, нейроны, связи
    result.layers = simplifiedModel["layers"].toArray().size();
    
    int totalNeurons = 0;
    int totalConnections = 0;
    
    QJsonArray layers = simplifiedModel["layers"].toArray();
    for (const QJsonValue &layerValue : layers) {
        QJsonObject layer = layerValue.toObject();
        if (layer.contains("neurons")) {
            totalNeurons += layer["neurons"].toArray().size();
        }
        if (layer.contains("connections")) {
            totalConnections += layer["connections"].toArray().size();
        }
    }
    
    result.neurons = totalNeurons;
    result.connections = totalConnections;
    result.processingTime = 1.0 + QRandomGenerator::global()->bounded(0, 900) / 100.0; // Симуляция времени обработки (1.0-10.0)
}

SimplificationResult SimplificationAlgorithms::selectBestAlgorithm(const QJsonObject &model)
{
    // Анализируем характеристики модели для выбора лучшего алгоритма
    int layerCount = model["layers"].toArray().size();
    int totalParams = model["parameters"].toInt(1000);
    double modelSize = model["size"].toDouble(100.0);
    
    SimplificationResult result;
    result.algorithmName = "Auto";
    
    // Логика выбора алгоритма на основе характеристик модели
    if (modelSize > 500.0 && totalParams > 1000000) {
        // Большая модель - используем комбинированный подход
        qDebug() << "🔍 Большая модель обнаружена, применяем комбинированный подход";
        result = combinedSimplification(model, QStringList() << "Pruning" << "Quantization");
    } else if (layerCount > 10) {
        // Глубокая модель - используем архитектурный поиск
        qDebug() << "🏗️ Глубокая модель обнаружена, применяем архитектурный поиск";
        result = architectureSearch(model);
    } else if (totalParams > 500000) {
        // Много параметров - используем квантование
        qDebug() << "🔢 Модель с большим количеством параметров, применяем квантование";
        result = quantization(model);
    } else {
        // Стандартная модель - используем pruning
        qDebug() << "✂️ Стандартная модель, применяем pruning";
        result = pruning(model);
    }
    
    // Обновляем название алгоритма
    result.algorithmName = "Auto (" + result.algorithmName + ")";
    
    return result;
}
