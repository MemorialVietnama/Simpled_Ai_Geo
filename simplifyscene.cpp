#include "simplifyscene.h"
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QApplication>
#include <QMessageBox>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QTime>
#include <QPainter>
#include <QPen>
#include <QBrush>
#include <cmath>
#include <cstdlib>
#include <algorithm>

// ============================================================================
// NeuralNetwork2DWidget Implementation
// ============================================================================

NeuralNetwork2DWidget::NeuralNetwork2DWidget(QWidget *parent)
    : QWidget(parent)
    , rotationX(0.0f)
    , rotationY(0.0f)
    , zoom(1.0f)
    , isDragging(false)
    , center(0, 0)
    , hoveredNeuron(-1)
{
    setMinimumSize(400, 300);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
}

NeuralNetwork2DWidget::~NeuralNetwork2DWidget()
{
}

void NeuralNetwork2DWidget::setNeurons(const QVector<Neuron> &neurons)
{
    this->neurons = neurons;
    update();
}

void NeuralNetwork2DWidget::setConnections(const QVector<Connection> &connections)
{
    this->connections = connections;
    update();
}

void NeuralNetwork2DWidget::updateNeuron(int index, const Neuron &neuron)
{
    if (index >= 0 && index < neurons.size()) {
        neurons[index] = neuron;
        update();
    }
}

void NeuralNetwork2DWidget::updateConnection(int index, const Connection &connection)
{
    if (index >= 0 && index < connections.size()) {
        connections[index] = connection;
        update();
    }
}

void NeuralNetwork2DWidget::paintEvent(QPaintEvent * /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Очищаем фон
    painter.fillRect(rect(), QColor(20, 20, 30));
    
    // Обновляем центр экрана
    center = QPointF(width() / 2.0, height() / 2.0);
    
    // Рисуем связи
    drawConnections(painter);
    
    // Рисуем нейроны
    drawNeurons(painter);
    
    // Рисуем подсказки
    drawTooltips(painter);
}

void NeuralNetwork2DWidget::drawNeurons(QPainter &painter)
{
    // Сортируем нейроны по глубине для правильного отображения
    QVector<QPair<float, int>> depthSorted;
    for (int i = 0; i < neurons.size(); ++i) {
        if (!neurons[i].isActive) continue;
        
        // QPoint screenPos = worldToScreen(neurons[i].position); // Не используется
        float cosX = cos(rotationX * M_PI / 180.0f);
        float sinX = sin(rotationX * M_PI / 180.0f);
        float cosY = cos(rotationY * M_PI / 180.0f);
        float sinY = sin(rotationY * M_PI / 180.0f);
        
        // Применяем поворот по Y
        // float x1 = neurons[i].position.x() * cosY - neurons[i].position.z() * sinY; // Не используется
        float z1 = neurons[i].position.x() * sinY + neurons[i].position.z() * cosY;
        
        // Применяем поворот по X
        float z = neurons[i].position.y() * sinX + z1 * cosX;
        
        depthSorted.append(qMakePair(z, i));
    }
    
    // Сортируем по глубине (дальние нейроны рисуем первыми)
    std::sort(depthSorted.begin(), depthSorted.end());
    
    for (const auto &pair : depthSorted) {
        int i = pair.second;
        const Neuron &neuron = neurons[i];
        
        // QPoint screenPos = worldToScreen(neuron.position); // Не используется
        
        // Вычисляем глубину для эффектов
        float cosX = cos(rotationX * M_PI / 180.0f);
        float sinX = sin(rotationX * M_PI / 180.0f);
        float cosY = cos(rotationY * M_PI / 180.0f);
        float sinY = sin(rotationY * M_PI / 180.0f);
        
        // Применяем поворот по Y
        // float x1 = neuron.position.x() * cosY - neuron.position.z() * sinY; // Не используется
        float z1 = neuron.position.x() * sinY + neuron.position.z() * cosY;
        
        // Применяем поворот по X
        float z = neuron.position.y() * sinX + z1 * cosX;
        float depth = (z + 2.0f) / 4.0f; // Нормализуем от 0 до 1
        depth = qBound(0.0f, depth, 1.0f);
        
        // Проекция на экран
        QPoint screenPos = worldToScreen(neuron.position);
        
        // Размер зависит от глубины
        int radius = static_cast<int>(neuron.size * 25 * zoom * (0.5f + 0.5f * depth));
        radius = qMax(3, radius); // Минимальный размер
        
        // Цвет нейрона с учетом глубины
        QColor neuronColor(
            static_cast<int>(neuron.color.x() * 255 * (0.7f + 0.3f * depth)),
            static_cast<int>(neuron.color.y() * 255 * (0.7f + 0.3f * depth)),
            static_cast<int>(neuron.color.z() * 255 * (0.7f + 0.3f * depth))
        );
        
        // Рисуем тень (для дальних нейронов)
        if (depth < 0.5f) {
            painter.setBrush(QBrush(QColor(0, 0, 0, 50)));
            painter.setPen(Qt::NoPen);
            painter.drawEllipse(screenPos + QPoint(2, 2), radius, radius);
        }
        
        // Рисуем нейрон с градиентом
        QRadialGradient gradient(screenPos, radius);
        gradient.setColorAt(0, neuronColor.lighter(150));
        gradient.setColorAt(1, neuronColor.darker(120));
        
        painter.setBrush(QBrush(gradient));
        painter.setPen(QPen(QColor(255, 255, 255, 150), 1));
        painter.drawEllipse(screenPos, radius, radius);
        
        // Рисуем номер слоя только для ближних нейронов
        if (depth > 0.3f && radius > 8) {
            painter.setPen(QPen(Qt::white, 1));
            painter.setFont(QFont("Arial", qMax(6, radius / 3)));
            painter.drawText(screenPos + QPoint(-radius/2, radius/4), QString::number(neuron.layer));
        }
    }
}

void NeuralNetwork2DWidget::drawConnections(QPainter &painter)
{
    // Сортируем связи по глубине для правильного отображения
    QVector<QPair<float, int>> depthSorted;
    for (int i = 0; i < connections.size(); ++i) {
        const Connection &connection = connections[i];
        if (!connection.isActive) continue;
        if (connection.fromNeuron >= neurons.size() || connection.toNeuron >= neurons.size()) continue;
        
        const Neuron &fromNeuron = neurons[connection.fromNeuron];
        const Neuron &toNeuron = neurons[connection.toNeuron];
        
        if (!fromNeuron.isActive || !toNeuron.isActive) continue;
        
        // Вычисляем среднюю глубину связи
        float cosX = cos(rotationX * M_PI / 180.0f);
        float sinX = sin(rotationX * M_PI / 180.0f);
        float cosY = cos(rotationY * M_PI / 180.0f);
        float sinY = sin(rotationY * M_PI / 180.0f);
        
        // Для fromNeuron
        // float x1_from = fromNeuron.position.x() * cosY - fromNeuron.position.z() * sinY; // Не используется
        float z1_from = fromNeuron.position.x() * sinY + fromNeuron.position.z() * cosY;
        float z1 = fromNeuron.position.y() * sinX + z1_from * cosX;
        
        // Для toNeuron
        // float x1_to = toNeuron.position.x() * cosY - toNeuron.position.z() * sinY; // Не используется
        float z1_to = toNeuron.position.x() * sinY + toNeuron.position.z() * cosY;
        float z2 = toNeuron.position.y() * sinX + z1_to * cosX;
        float avgZ = (z1 + z2) / 2.0f;
        
        depthSorted.append(qMakePair(avgZ, i));
    }
    
    // Сортируем по глубине
    std::sort(depthSorted.begin(), depthSorted.end());
    
    for (const auto &pair : depthSorted) {
        int i = pair.second;
        const Connection &connection = connections[i];
        
        const Neuron &fromNeuron = neurons[connection.fromNeuron];
        const Neuron &toNeuron = neurons[connection.toNeuron];
        
        QPoint fromPos = worldToScreen(fromNeuron.position);
        QPoint toPos = worldToScreen(toNeuron.position);
        
        // Вычисляем глубину для эффектов
        float cosX = cos(rotationX * M_PI / 180.0f);
        float sinX = sin(rotationX * M_PI / 180.0f);
        float cosY = cos(rotationY * M_PI / 180.0f);
        float sinY = sin(rotationY * M_PI / 180.0f);
        
        // Для fromNeuron
        // float x1_from = fromNeuron.position.x() * cosY - fromNeuron.position.z() * sinY; // Не используется
        float z1_from = fromNeuron.position.x() * sinY + fromNeuron.position.z() * cosY;
        float z1 = fromNeuron.position.y() * sinX + z1_from * cosX;
        
        // Для toNeuron
        // float x1_to = toNeuron.position.x() * cosY - toNeuron.position.z() * sinY; // Не используется
        float z1_to = toNeuron.position.x() * sinY + toNeuron.position.z() * cosY;
        float z2 = toNeuron.position.y() * sinX + z1_to * cosX;
        float avgZ = (z1 + z2) / 2.0f;
        float depth = (avgZ + 2.0f) / 4.0f;
        depth = qBound(0.0f, depth, 1.0f);
        
        // Цвет связи зависит от веса и глубины
        float weight = connection.weight;
        int alpha = static_cast<int>((abs(weight) * 150 + 30) * (0.5f + 0.5f * depth));
        alpha = qBound(30, alpha, 255);
        
        QColor lineColor;
        if (weight >= 0) {
            lineColor = QColor(100, 200, 255, alpha);
        } else {
            lineColor = QColor(255, 100, 100, alpha);
        }
        
        // Толщина линии зависит от веса и глубины
        int lineWidth = static_cast<int>((abs(weight) * 2 + 1) * (0.5f + 0.5f * depth));
        lineWidth = qMax(1, lineWidth);
        
        painter.setPen(QPen(lineColor, lineWidth));
        painter.drawLine(fromPos, toPos);
    }
}

QPoint NeuralNetwork2DWidget::worldToScreen(const QVector3D &worldPos)
{
    // 3D проекция с поворотом по X и Y
    float cosX = cos(rotationX * M_PI / 180.0f);
    float sinX = sin(rotationX * M_PI / 180.0f);
    float cosY = cos(rotationY * M_PI / 180.0f);
    float sinY = sin(rotationY * M_PI / 180.0f);
    
    // Применяем поворот по Y (горизонтальный)
    float x1 = worldPos.x() * cosY - worldPos.z() * sinY;
    float z1 = worldPos.x() * sinY + worldPos.z() * cosY;
    
    // Применяем поворот по X (вертикальный)
    float y = worldPos.y() * cosX - z1 * sinX;
    float z = worldPos.y() * sinX + z1 * cosX;
    
    // Простая перспективная проекция
    float perspective = 1.0f / (1.0f + z * 0.1f);
    
    // Проекция на экран с центрированием
    int screenX = static_cast<int>(center.x() + x1 * 80 * zoom * perspective);
    int screenY = static_cast<int>(center.y() + y * 80 * zoom * perspective);
    
    return QPoint(screenX, screenY);
}

QVector3D NeuralNetwork2DWidget::screenToWorld(const QPoint &screenPos)
{
    // Обратная проекция
    float worldX = (screenPos.x() - center.x()) / (100.0f * zoom);
    float worldY = (screenPos.y() - center.y()) / (100.0f * zoom);
    
    return QVector3D(worldX, worldY, 0);
}

void NeuralNetwork2DWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        isDragging = true;
        lastMousePos = event->pos();
        setCursor(Qt::ClosedHandCursor); // Показываем, что перетаскиваем
    }
}

void NeuralNetwork2DWidget::mouseMoveEvent(QMouseEvent *event)
{
    mousePos = event->pos();
    hoveredNeuron = getNeuronAt(mousePos);
    
    if (isDragging && (event->buttons() & Qt::LeftButton)) {
        int dx = event->pos().x() - lastMousePos.x();
        int dy = event->pos().y() - lastMousePos.y();
        
        // Поворот по Y (горизонтальное движение мыши)
        rotationY += dx * 0.5f;
        
        // Поворот по X (вертикальное движение мыши)
        rotationX += dy * 0.5f;
        
        // Ограничиваем поворот по Y
        if (rotationY > 89.0f) rotationY = 89.0f;
        if (rotationY < -89.0f) rotationY = -89.0f;
        
        // Ограничиваем поворот по X
        if (rotationX > 89.0f) rotationX = 89.0f;
        if (rotationX < -89.0f) rotationX = -89.0f;
        
        update();
        lastMousePos = event->pos();
    } else {
        update(); // Обновляем для подсказок
    }
}

void NeuralNetwork2DWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        isDragging = false;
        setCursor(Qt::ArrowCursor); // Возвращаем обычный курсор
    }
}

void NeuralNetwork2DWidget::wheelEvent(QWheelEvent *event)
{
    float delta = event->angleDelta().y() / 120.0f;
    zoom += delta * 0.1f;
    
    if (zoom < 0.1f) zoom = 0.1f;
    if (zoom > 3.0f) zoom = 3.0f;
    
    update();
}

// ============================================================================
// SimplifyScene Implementation
// ============================================================================

SimplifyScene::SimplifyScene(QWidget *parent)
    : QWidget(parent)
    , backButton(nullptr)
    , startButton(nullptr)
    , titleLabel(nullptr)
    , statusLabel(nullptr)
    , network2DWidget(nullptr)
    , loaderLabel(nullptr)
    , loaderProgress(nullptr)
    , loaderTimer(nullptr)
    , loaderStep(0)
    , logOutput(nullptr)
    , isSimplificationRunning(false)
    , algorithms(new SimplificationAlgorithms(this))
{
    // Устанавливаем минимальный размер окна
    setMinimumSize(800, 600);
    
    setupUI();
    setup2DVisualization();
    setupLogWindow();
}

SimplifyScene::~SimplifyScene()
{
    if (loaderTimer) {
        loaderTimer->stop();
    }
}

void SimplifyScene::setupUI()
{
    this->setStyleSheet("QWidget { background-color: #ffffff; }");
    
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(16);
    mainLayout->setContentsMargins(24, 24, 24, 24);

    // Header
    QHBoxLayout *headerLayout = new QHBoxLayout();
    
    backButton = std::make_unique<QPushButton>("← Назад");
    backButton->setObjectName("backButton");
    backButton->setMaximumWidth(100);
    backButton->setMinimumHeight(40);
    backButton->setStyleSheet(R"(
        QPushButton {
            background-color: #f5f5f5;
            color: #333333;
            border: 1px solid #e0e0e0;
            border-radius: 6px;
            padding: 8px 16px;
            font-weight: 500;
            font-size: 14px;
        }
        QPushButton:hover {
            background-color: #eeeeee;
            border-color: #cccccc;
        }
        QPushButton:pressed {
            background-color: #e0e0e0;
        }
    )");
    headerLayout->addWidget(backButton.get());
    
    headerLayout->addStretch();
    
    titleLabel = std::make_unique<QLabel>("Упрощение модели");
    titleLabel->setStyleSheet(R"(
        font-size: 24px; 
        font-weight: 600; 
        color: #333333;
        padding: 8px 0;
    )");
    headerLayout->addWidget(titleLabel.get());
    
    headerLayout->addStretch();
    
    mainLayout->addLayout(headerLayout);


    // Control buttons - перемещаем наверх
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(12);
    
    startButton = std::make_unique<QPushButton>("🚀 Начать упрощение");
    startButton->setStyleSheet(R"(
        QPushButton {
            background-color: #28a745;
            color: white;
            border: none;
            border-radius: 6px;
            padding: 8px 16px;
            font-weight: 500;
            font-size: 12px;
            min-height: 32px;
            min-width: 120px;
        }
        QPushButton:hover {
            background-color: #218838;
        }
        QPushButton:pressed {
            background-color: #1e7e34;
        }
        QPushButton:disabled {
            background-color: #cccccc;
            color: #999999;
        }
    )");
    
    buttonLayout->addWidget(startButton.get());
    buttonLayout->addStretch();
    
    mainLayout->addLayout(buttonLayout);
    
    // Лоадер под кнопкой
    QHBoxLayout *loaderLayout = new QHBoxLayout();
    loaderLayout->setSpacing(12);
    
    loaderLabel = std::make_unique<QLabel>("Готов к упрощению");
    loaderLabel->setStyleSheet("font-size: 14px; color: #666666;");
    loaderLayout->addWidget(loaderLabel.get());
    
    loaderProgress = std::make_unique<QProgressBar>();
    loaderProgress->setRange(0, 100);
    loaderProgress->setValue(0);
    loaderProgress->setVisible(false);
    loaderProgress->setStyleSheet(R"(
        QProgressBar {
            border: 1px solid #e0e0e0;
            border-radius: 6px;
            text-align: center;
            background-color: #f8f8f8;
            height: 20px;
        }
        QProgressBar::chunk {
            background-color: #28a745;
            border-radius: 5px;
        }
    )");
    loaderLayout->addWidget(loaderProgress.get());
    
    loaderLayout->addStretch();
    mainLayout->addLayout(loaderLayout);

    // 3D Visualization Area - добавляем после кнопок
    QLabel *vizLabel = new QLabel("3D Визуализация нейронной сети (поворот по X и Y осям)");
    vizLabel->setStyleSheet("font-size: 16px; font-weight: 600; color: #333333; margin-bottom: 8px;");
    mainLayout->addWidget(vizLabel);
    
    network2DWidget = std::make_unique<NeuralNetwork2DWidget>();
    network2DWidget->setFixedHeight(400); // Фиксированная высота для 3D окна
    network2DWidget->setStyleSheet(R"(
        QWidget {
            border: 2px solid #e0e0e0;
            border-radius: 8px;
            background-color: #f8f8f8;
        }
    )");
    mainLayout->addWidget(network2DWidget.get());

    // Connect signals
    connect(backButton.get(), &QPushButton::clicked, this, &SimplifyScene::onBackClicked);
    connect(startButton.get(), &QPushButton::clicked, this, &SimplifyScene::onStartSimplification);
    
    // Подключаем сигналы от алгоритмов
    connect(algorithms, &SimplificationAlgorithms::progressUpdated, this, &SimplifyScene::onProgressUpdated);
    connect(algorithms, &SimplificationAlgorithms::algorithmFinished, this, &SimplifyScene::onAlgorithmFinished);
    connect(algorithms, &SimplificationAlgorithms::algorithmError, this, &SimplifyScene::onAlgorithmError);
}

void SimplifyScene::setup2DVisualization()
{
    // Генерируем тестовую нейронную сеть
    generateNeuralNetwork();
}

void SimplifyScene::setupLoader()
{
    // Настройка динамического лоадера
    loaderLabel = std::make_unique<QLabel>("Готов к работе");
    loaderLabel->setStyleSheet(R"(
        font-size: 16px; 
        font-weight: 500; 
        color: #333333;
        padding: 16px;
        background-color: #f8f9fa;
        border: 1px solid #e0e0e0;
        border-radius: 6px;
        margin: 8px 0;
    )");
    loaderLabel->setAlignment(Qt::AlignCenter);
    loaderLabel->setMinimumHeight(60);
    
    loaderProgress = std::make_unique<QProgressBar>();
    loaderProgress->setStyleSheet(R"(
        QProgressBar {
            border: 1px solid #e0e0e0;
            border-radius: 6px;
            background-color: #f8f9fa;
            text-align: center;
            font-weight: 500;
        }
        QProgressBar::chunk {
            background-color: #007bff;
            border-radius: 5px;
        }
    )");
    loaderProgress->setVisible(false);
    
    // Настройка таймера для анимации
    loaderTimer = std::make_unique<QTimer>(this);
    connect(loaderTimer.get(), &QTimer::timeout, this, &SimplifyScene::updateLoaderText);
    
    // Сообщения для лоадера
    loaderMessages = {
        "🔍 Анализ архитектуры модели...",
        "📊 Вычисление важности нейронов...",
        "🎯 Определение избыточных связей...",
        "✂️ Удаление слабых соединений...",
        "🔄 Пересчет весов...",
        "📈 Оценка качества упрощения...",
        "💾 Сохранение оптимизированной модели...",
        "✅ Упрощение завершено!"
    };
}

void SimplifyScene::setupLogWindow()
{
    QLabel *logLabel = new QLabel("Лог операций");
    logLabel->setStyleSheet("font-size: 16px; font-weight: 600; color: #333333; margin-bottom: 8px;");
    
    logOutput = std::make_unique<QTextEdit>();
    logOutput->setReadOnly(true);
    logOutput->setFixedHeight(100); // Фиксированная высота лога
    logOutput->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded); // Добавляем скролл
    logOutput->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    logOutput->setFont(QFont("Consolas", 10));
    logOutput->setStyleSheet(R"(
        QTextEdit {
            background-color: #f8f8f8;
            color: #333333;
            border: 1px solid #e0e0e0;
            border-radius: 6px;
            padding: 12px;
            font-family: 'Consolas', 'Monaco', 'Courier New', monospace;
            font-size: 11px;
            line-height: 1.4;
        }
    )");
    
    // Добавляем в основной layout
    QVBoxLayout *mainLayout = qobject_cast<QVBoxLayout*>(this->layout());
    if (mainLayout) {
        // Добавляем лог перед 3D виджетом, но после кнопок
        // Найдем индекс 3D виджета и вставим лог перед ним
        int networkIndex = mainLayout->indexOf(network2DWidget.get());
        if (networkIndex >= 0) {
            mainLayout->insertWidget(networkIndex, logLabel);
            mainLayout->insertWidget(networkIndex + 1, logOutput.get());
        } else {
            // Если не найден, добавляем в конец
            mainLayout->addWidget(logLabel);
            mainLayout->addWidget(logOutput.get());
        }
    }
}

void SimplifyScene::generateNeuralNetwork()
{
    neurons.clear();
    connections.clear();
    
    // Генерируем 100 нейронов в 3D пространстве
    int neuronsPerLayer = 10;
    int numLayers = 10;
    
    for (int layer = 0; layer < numLayers; layer++) {
        for (int i = 0; i < neuronsPerLayer; i++) {
            Neuron neuron;
            
            // Создаем более интересную 3D структуру
            float layerSpacing = 1.0f;
            float neuronSpacing = 0.4f;
            
            // Располагаем нейроны в 3D пространстве с более интересной геометрией
            float x = (i - neuronsPerLayer/2.0f) * neuronSpacing;
            float y = layer * layerSpacing - (numLayers * layerSpacing) / 2.0f; // Центрируем по Y
            
            // Создаем более сложную 3D структуру
            float radius = 0.5f + layer * 0.1f; // Радиус увеличивается с каждым слоем
            float angle = (i * 2.0f * M_PI) / neuronsPerLayer; // Угол для кругового расположения
            float z = radius * cos(angle) + sin(layer * 0.4f) * 0.3f; // Круговое + волновое расположение
            
            neuron.position = QVector3D(x, y, z);
            neuron.size = 0.15f + (rand() % 15) / 100.0f;
            
            // Отладочная информация (можно убрать в релизе)
            if (layer == 0 && i < 3) {
                qDebug() << QString("Нейрон [%1,%2]: позиция (%3, %4, %5)")
                    .arg(layer).arg(i)
                    .arg(x, 0, 'f', 2)
                    .arg(y, 0, 'f', 2)
                    .arg(z, 0, 'f', 2);
            }
            
            // Цветовая схема: от синего к красному через слои
            float layerRatio = float(layer) / (numLayers - 1);
            neuron.color = QVector3D(
                0.2f + layerRatio * 0.8f,  // R: от 0.2 до 1.0
                0.3f + (1.0f - layerRatio) * 0.7f,  // G: от 1.0 до 0.3
                0.8f + layerRatio * 0.2f   // B: от 0.8 до 1.0
            );
            
            neuron.layer = layer;
            neuron.index = i;
            neuron.isActive = true;
            
            neurons.append(neuron);
        }
    }
    
    // Создаем связи между слоями
    for (int layer = 0; layer < numLayers - 1; layer++) {
        for (int from = 0; from < neuronsPerLayer; from++) {
            for (int to = 0; to < neuronsPerLayer; to++) {
                Connection conn;
                conn.fromNeuron = layer * neuronsPerLayer + from;
                conn.toNeuron = (layer + 1) * neuronsPerLayer + to;
                conn.weight = (rand() % 100) / 100.0f - 0.5f;
                conn.isActive = true;
                
                connections.append(conn);
            }
        }
    }
    
    // Обновляем 2D виджет
    if (network2DWidget) {
        network2DWidget->setNeurons(neurons);
        network2DWidget->setConnections(connections);
    }
    
    addLogMessage("🎯 Сгенерирована нейронная сеть: " + QString::number(neurons.size()) + " нейронов, " + QString::number(connections.size()) + " связей");
}

void SimplifyScene::updateNeuralNetwork()
{
    if (network2DWidget) {
        network2DWidget->setNeurons(neurons);
        network2DWidget->setConnections(connections);
    }
}

void SimplifyScene::addLogMessage(const QString &message)
{
    if (logOutput) {
        QString timestamp = QTime::currentTime().toString("hh:mm:ss");
        logOutput->append(QString("[%1] %2").arg(timestamp).arg(message));
        
        // Автопрокрутка вниз
        QTextCursor cursor = logOutput->textCursor();
        cursor.movePosition(QTextCursor::End);
        logOutput->setTextCursor(cursor);
    }
}

void SimplifyScene::onProgressUpdated(int percentage, const QString &message)
{
    if (loaderProgress) {
        loaderProgress->setValue(percentage);
    }
    if (loaderLabel) {
        loaderLabel->setText(QString("📊 %1% - %2").arg(percentage).arg(message));
    }
    addLogMessage(QString("📊 %1% - %2").arg(percentage).arg(message));
}

void SimplifyScene::onAlgorithmFinished(const QString &algorithmName, const SimplificationResult &result)
{
    addLogMessage(QString("✅ Алгоритм %1 завершен успешно!").arg(algorithmName));
    addLogMessage(QString("📈 Результаты: сжатие %1%%, потеря точности %2%%, размер %3 МБ")
                  .arg(result.sizeReduction, 0, 'f', 1)
                  .arg(result.accuracyLoss, 0, 'f', 2)
                  .arg(result.modelSizeMB, 0, 'f', 1));
    
    // Сохраняем результаты
    simplifiedModelData = result.simplifiedModel;
    simplificationResult = QJsonObject{
        {"algorithm", algorithmName},
        {"compressionRatio", result.compressionRatio},
        {"accuracyLoss", result.accuracyLoss},
        {"sizeReduction", result.sizeReduction},
        {"totalParameters", result.totalParameters},
        {"modelSizeMB", result.modelSizeMB},
        {"layers", result.layers},
        {"neurons", result.neurons},
        {"connections", result.connections},
        {"processingTime", result.processingTime}
    };
    
    // Обновляем UI
    startButton->setEnabled(true);
    loaderProgress->setVisible(false);
    loaderLabel->setText("✅ Упрощение завершено!");
    isSimplificationRunning = false;
    
    // Обновляем визуализацию
    updateNeuralNetwork();
    
    // Автоматически переходим к сравнению
    addLogMessage("🔄 Автоматический переход к сравнению моделей...");
    
    qDebug() << "SimplifyScene - испускаем сигнал comparisonRequested:";
    qDebug() << "  - originalModelData пуста:" << originalModelData.isEmpty();
    qDebug() << "  - simplifiedModelData пуста:" << simplifiedModelData.isEmpty();
    qDebug() << "  - simplificationResult пуст:" << simplificationResult.isEmpty();
    
    emit comparisonRequested(originalModelData, simplifiedModelData, simplificationResult);
}

void SimplifyScene::onAlgorithmError(const QString &error)
{
    addLogMessage(QString("❌ Ошибка алгоритма: %1").arg(error));
    
    // Обновляем UI
    startButton->setEnabled(true);
    loaderProgress->setVisible(false);
    loaderLabel->setText("❌ Ошибка упрощения");
    isSimplificationRunning = false;
}

void SimplifyScene::onShowComparison()
{
    if (originalModelData.isEmpty() || simplifiedModelData.isEmpty()) {
        addLogMessage("❌ Нет данных для сравнения");
        return;
    }
    
    addLogMessage("🔄 Переход к сравнению моделей...");
    emit comparisonRequested(originalModelData, simplifiedModelData, simplificationResult);
}

void SimplifyScene::startAllAlgorithms()
{
    // Список всех алгоритмов для последовательного выполнения
    QStringList algorithmList = {
        "Douglas-Peucker",
        "Visvalingam-Whyatt", 
        "Pruning",
        "Quantization",
        "Knowledge Distillation",
        "Low-Rank Decomposition",
        "Architecture Search",
        "Combined"
    };
    
    addLogMessage(QString("🔄 Будет выполнено %1 алгоритмов упрощения").arg(algorithmList.size()));
    
    // Запускаем первый алгоритм
    if (!algorithmList.isEmpty()) {
        algorithms->startSimplification(algorithmList.first(), currentModelData);
    }
}

void SimplifyScene::setModelData(const QJsonObject &modelData)
{
    currentModelData = modelData;
    addLogMessage("📊 Загружены данные модели для упрощения");
    
    // Отладочная информация
    qDebug() << "SimplifyScene::setModelData - данные модели:";
    qDebug() << "  - Пусто ли:" << modelData.isEmpty();
    qDebug() << "  - Ключи:" << modelData.keys();
    if (modelData.contains("layers")) {
        qDebug() << "  - Количество слоев:" << modelData["layers"].toArray().size();
    }
    if (modelData.contains("parameters")) {
        qDebug() << "  - Параметры:" << modelData["parameters"].toInt();
    }
}

void SimplifyScene::startSimplification()
{
    if (isSimplificationRunning) return;
    
    isSimplificationRunning = true;
    startButton->setEnabled(false);
    loaderProgress->setVisible(true);
    loaderProgress->setValue(0);
    
    loaderStep = 0;
    loaderTimer->start(2000); // Обновляем каждые 2 секунды
    
    addLogMessage("🚀 Начато упрощение модели");
}

void SimplifyScene::stopSimplification()
{
    if (!isSimplificationRunning) return;
    
    isSimplificationRunning = false;
    loaderTimer->stop();
    
    startButton->setEnabled(true);
    loaderProgress->setVisible(false);
    loaderLabel->setText("Упрощение остановлено");
    
    addLogMessage("⏹️ Упрощение остановлено пользователем");
}

void SimplifyScene::updateLoaderText()
{
    if (loaderStep < loaderMessages.size()) {
        loaderLabel->setText(loaderMessages[loaderStep]);
        loaderProgress->setValue((loaderStep + 1) * 100 / loaderMessages.size());
        
        addLogMessage(loaderMessages[loaderStep]);
        
        // Симуляция изменений в нейронной сети
        if (loaderStep > 2 && loaderStep < 6) {
            // Удаляем некоторые связи
            for (int i = 0; i < connections.size() && i < 10; i++) {
                if (connections[i].isActive && rand() % 3 == 0) {
                    connections[i].isActive = false;
                }
            }
            updateNeuralNetwork();
        }
        
        loaderStep++;
    } else {
        // Завершение
        loaderTimer->stop();
        isSimplificationRunning = false;
        startButton->setEnabled(true);
        loaderProgress->setVisible(false);
        loaderLabel->setText("Упрощение завершено!");
        
        addLogMessage("✅ Упрощение модели успешно завершено!");
        emit simplificationFinished();
    }
}

void SimplifyScene::onBackClicked()
{
    emit backRequested();
}

void SimplifyScene::onStartSimplification()
{
    qDebug() << "SimplifyScene::onStartSimplification - проверка данных:";
    qDebug() << "  - algorithms:" << (algorithms != nullptr);
    qDebug() << "  - currentModelData.isEmpty():" << currentModelData.isEmpty();
    qDebug() << "  - currentModelData keys:" << currentModelData.keys();
    
    if (!algorithms || currentModelData.isEmpty()) {
        addLogMessage("❌ Ошибка: Нет данных модели или алгоритмов");
        addLogMessage(QString("   - Алгоритмы: %1").arg(algorithms ? "✅" : "❌"));
        addLogMessage(QString("   - Данные модели: %1").arg(currentModelData.isEmpty() ? "❌" : "✅"));
        return;
    }
    
    addLogMessage("🚀 Запуск автоматического упрощения: Все алгоритмы будут применены последовательно");
    
    // Сохраняем оригинальную модель
    originalModelData = currentModelData;
    
    // Показываем лоадер
    loaderProgress->setVisible(true);
    loaderProgress->setValue(0);
    loaderLabel->setText("🚀 Запуск упрощения...");
    startButton->setEnabled(false);
    
    // Запускаем все алгоритмы последовательно
    startAllAlgorithms();
    
    isSimplificationRunning = true;
}

void SimplifyScene::onStopSimplification()
{
    stopSimplification();
}

void SimplifyScene::onSimplificationStep()
{
    updateLoaderText();
}

// ============================================================================
// NeuralNetwork2DWidget Tooltip Methods
// ============================================================================

void NeuralNetwork2DWidget::drawTooltips(QPainter &painter)
{
    if (hoveredNeuron >= 0 && hoveredNeuron < neurons.size()) {
        const Neuron &neuron = neurons[hoveredNeuron];
        // QPoint screenPos = worldToScreen(neuron.position); // Не используется
        
        // Создаем текст подсказки с реальными координатами и дополнительной информацией
        QString tooltipText = QString("Слой: %1\nНейрон: %2\nПозиция: (%3, %4, %5)\nРазмер: %6\nАктивен: %7")
            .arg(neuron.layer)
            .arg(neuron.index)
            .arg(neuron.position.x(), 0, 'f', 2)
            .arg(neuron.position.y(), 0, 'f', 2)
            .arg(neuron.position.z(), 0, 'f', 2)
            .arg(neuron.size, 0, 'f', 2)
            .arg(neuron.isActive ? "Да" : "Нет");
        
        // Настраиваем шрифт
        QFont font("Arial", 10);
        painter.setFont(font);
        
        // Вычисляем размер текста
        QFontMetrics fm(font);
        QRect textRect = fm.boundingRect(QRect(0, 0, 200, 100), Qt::TextWordWrap, tooltipText);
        textRect.adjust(-8, -4, 8, 4); // Добавляем отступы
        
        // Позиционируем подсказку рядом с курсором
        QPoint tooltipPos = mousePos + QPoint(15, 15);
        textRect.moveTopLeft(tooltipPos);
        
        // Рисуем фон подсказки
        painter.setBrush(QBrush(QColor(0, 0, 0, 200)));
        painter.setPen(QPen(QColor(255, 255, 255, 150), 1));
        painter.drawRoundedRect(textRect, 4, 4);
        
        // Рисуем текст
        painter.setPen(QPen(Qt::white, 1));
        painter.drawText(textRect, Qt::AlignLeft | Qt::AlignTop, tooltipText);
    }
}

int NeuralNetwork2DWidget::getNeuronAt(const QPoint &screenPos)
{
    for (int i = 0; i < neurons.size(); ++i) {
        if (!neurons[i].isActive) continue;
        
        QPoint neuronPos = worldToScreen(neurons[i].position);
        int radius = static_cast<int>(neurons[i].size * 25 * zoom);
        
        // Проверяем, находится ли курсор в радиусе нейрона
        QPoint diff = screenPos - neuronPos;
        int distance = static_cast<int>(sqrt(diff.x() * diff.x() + diff.y() * diff.y()));
        
        if (distance <= radius + 5) { // Добавляем небольшой запас
            return i;
        }
    }
    return -1;
}