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
        
        QPoint screenPos = worldToScreen(neurons[i].position);
        float cosY = cos(rotationY * M_PI / 180.0f);
        float sinY = sin(rotationY * M_PI / 180.0f);
        float z = neurons[i].position.x() * sinY + neurons[i].position.z() * cosY;
        
        depthSorted.append(qMakePair(z, i));
    }
    
    // Сортируем по глубине (дальние нейроны рисуем первыми)
    std::sort(depthSorted.begin(), depthSorted.end());
    
    for (const auto &pair : depthSorted) {
        int i = pair.second;
        const Neuron &neuron = neurons[i];
        
        QPoint screenPos = worldToScreen(neuron.position);
        
        // Вычисляем глубину для эффектов
        float cosY = cos(rotationY * M_PI / 180.0f);
        float sinY = sin(rotationY * M_PI / 180.0f);
        float z = neuron.position.x() * sinY + neuron.position.z() * cosY;
        float depth = (z + 2.0f) / 4.0f; // Нормализуем от 0 до 1
        depth = qBound(0.0f, depth, 1.0f);
        
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
        float cosY = cos(rotationY * M_PI / 180.0f);
        float sinY = sin(rotationY * M_PI / 180.0f);
        float z1 = fromNeuron.position.x() * sinY + fromNeuron.position.z() * cosY;
        float z2 = toNeuron.position.x() * sinY + toNeuron.position.z() * cosY;
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
        float cosY = cos(rotationY * M_PI / 180.0f);
        float sinY = sin(rotationY * M_PI / 180.0f);
        float z1 = fromNeuron.position.x() * sinY + fromNeuron.position.z() * cosY;
        float z2 = toNeuron.position.x() * sinY + toNeuron.position.z() * cosY;
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
    // 3D проекция с поворотом по Y
    float cosY = cos(rotationY * M_PI / 180.0f);
    float sinY = sin(rotationY * M_PI / 180.0f);
    
    // Применяем поворот по Y (основной поворот)
    float x = worldPos.x() * cosY - worldPos.z() * sinY;
    float z = worldPos.x() * sinY + worldPos.z() * cosY;
    
    // Простая перспективная проекция
    float perspective = 1.0f / (1.0f + z * 0.1f);
    
    // Проекция на экран с центрированием
    int screenX = static_cast<int>(center.x() + x * 80 * zoom * perspective);
    int screenY = static_cast<int>(center.y() + worldPos.y() * 80 * zoom * perspective);
    
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
    }
}

void NeuralNetwork2DWidget::mouseMoveEvent(QMouseEvent *event)
{
    mousePos = event->pos();
    hoveredNeuron = getNeuronAt(mousePos);
    
    if (isDragging) {
        int dx = event->pos().x() - lastMousePos.x();
        rotationY += dx * 0.5f;
        
        // Ограничиваем поворот по Y
        if (rotationY > 89.0f) rotationY = 89.0f;
        if (rotationY < -89.0f) rotationY = -89.0f;
        
        update();
        lastMousePos = event->pos();
    } else {
        update(); // Обновляем для подсказок
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
    , stopButton(nullptr)
    , titleLabel(nullptr)
    , statusLabel(nullptr)
    , network2DWidget(nullptr)
    , loaderLabel(nullptr)
    , loaderProgress(nullptr)
    , loaderTimer(nullptr)
    , loaderStep(0)
    , logOutput(nullptr)
    , isSimplificationRunning(false)
{
    setupUI();
    setup2DVisualization();
    setupLoader();
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
    
    backButton = new QPushButton("← Назад");
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
    headerLayout->addWidget(backButton);
    
    headerLayout->addStretch();
    
    titleLabel = new QLabel("Упрощение модели");
    titleLabel->setStyleSheet(R"(
        font-size: 24px; 
        font-weight: 600; 
        color: #333333;
        padding: 8px 0;
    )");
    headerLayout->addWidget(titleLabel);
    
    headerLayout->addStretch();
    
    mainLayout->addLayout(headerLayout);

    // Убираем status label для расширения 3D окна

    // 3D Visualization Area
    QLabel *vizLabel = new QLabel("3D Визуализация нейронной сети (поворот только по Y-оси)");
    vizLabel->setStyleSheet("font-size: 16px; font-weight: 600; color: #333333; margin-bottom: 8px;");
    mainLayout->addWidget(vizLabel);
    
    network2DWidget = new NeuralNetwork2DWidget();
    network2DWidget->setMinimumHeight(500); // Увеличиваем высоту
    network2DWidget->setStyleSheet(R"(
        QWidget {
            border: 2px solid #e0e0e0;
            border-radius: 8px;
            background-color: #f8f8f8;
        }
    )");
    mainLayout->addWidget(network2DWidget);

    // Control buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(12);
    
    startButton = new QPushButton("Начать упрощение");
    startButton->setStyleSheet(R"(
        QPushButton {
            background-color: #28a745;
            color: white;
            border: none;
            border-radius: 6px;
            padding: 12px 24px;
            font-weight: 500;
            font-size: 14px;
            min-height: 40px;
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
    
    stopButton = new QPushButton("Остановить");
    stopButton->setStyleSheet(R"(
        QPushButton {
            background-color: #dc3545;
            color: white;
            border: none;
            border-radius: 6px;
            padding: 12px 24px;
            font-weight: 500;
            font-size: 14px;
            min-height: 40px;
        }
        QPushButton:hover {
            background-color: #c82333;
        }
        QPushButton:pressed {
            background-color: #bd2130;
        }
        QPushButton:disabled {
            background-color: #cccccc;
            color: #999999;
        }
    )");
    stopButton->setEnabled(false);
    
    buttonLayout->addWidget(startButton);
    buttonLayout->addWidget(stopButton);
    buttonLayout->addStretch();
    
    mainLayout->addLayout(buttonLayout);

    // Connect signals
    connect(backButton, &QPushButton::clicked, this, &SimplifyScene::onBackClicked);
    connect(startButton, &QPushButton::clicked, this, &SimplifyScene::onStartSimplification);
    connect(stopButton, &QPushButton::clicked, this, &SimplifyScene::onStopSimplification);
}

void SimplifyScene::setup2DVisualization()
{
    // Генерируем тестовую нейронную сеть
    generateNeuralNetwork();
}

void SimplifyScene::setupLoader()
{
    // Настройка динамического лоадера
    loaderLabel = new QLabel("Готов к работе");
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
    
    loaderProgress = new QProgressBar();
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
    loaderTimer = new QTimer(this);
    connect(loaderTimer, &QTimer::timeout, this, &SimplifyScene::updateLoaderText);
    
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
    
    logOutput = new QTextEdit();
    logOutput->setReadOnly(true);
    logOutput->setMaximumHeight(150);
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
        mainLayout->addWidget(logLabel);
        mainLayout->addWidget(logOutput);
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
            
            // Располагаем нейроны в 3D пространстве
            float x = (i - neuronsPerLayer/2.0f) * neuronSpacing;
            float y = layer * layerSpacing - (numLayers * layerSpacing) / 2.0f; // Центрируем по Y
            float z = sin(i * 0.5f) * 0.3f + cos(layer * 0.3f) * 0.2f; // Волнообразное расположение по Z
            
            neuron.position = QVector3D(x, y, z);
            neuron.size = 0.15f + (rand() % 15) / 100.0f;
            
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

void SimplifyScene::setModelData(const QJsonObject &modelData)
{
    currentModelData = modelData;
    addLogMessage("📊 Загружены данные модели для упрощения");
}

void SimplifyScene::startSimplification()
{
    if (isSimplificationRunning) return;
    
    isSimplificationRunning = true;
    startButton->setEnabled(false);
    stopButton->setEnabled(true);
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
    stopButton->setEnabled(false);
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
        stopButton->setEnabled(false);
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
    startSimplification();
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
        QPoint screenPos = worldToScreen(neuron.position);
        
        // Создаем текст подсказки
        QString tooltipText = QString("Слой: %1\nНейрон: %2\nПозиция: (%.2f, %.2f, %.2f)")
            .arg(neuron.layer)
            .arg(neuron.index)
            .arg(neuron.position.x())
            .arg(neuron.position.y())
            .arg(neuron.position.z());
        
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