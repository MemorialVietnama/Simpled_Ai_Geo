#include "comparisonscene.h"
#include "simplifyscene.h"  // Для доступа к NeuralNetworkPolygonWidget и структурам
#include <QDebug>
#include <QApplication>
#include <QMessageBox>
#include <QProcess>
#include <QHeaderView>
#include <QScrollArea>
#include <QGroupBox>
#include <QFormLayout>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QTextEdit>
#include <QFont>
#include <QFontMetrics>
#include <QPainterPath>
#include <QLinearGradient>
#include <QConicalGradient>
#include <QTime>
#include <QDateTime>
#include <QSignalBlocker>
#include <QTextCursor>
#include <cmath>
#include <set>

// ============================================================================
// ComparisonScene Implementation
// ============================================================================

ComparisonScene::ComparisonScene(QWidget *parent)
    : QWidget(parent)
    , polygonStackedWidget(nullptr)
    , originalLayerComboBoxPtr(nullptr)
    , simplifiedLayerComboBoxPtr(nullptr)
    , viewOriginalButton(nullptr)
    , viewSimplifiedButton(nullptr)
    , currentViewMode(0)  // По умолчанию показываем оригинал
{
    setupUI();
}

ComparisonScene::~ComparisonScene() = default;

void ComparisonScene::setupUI()
{
    setStyleSheet(R"(
        QWidget {
            background-color: #f8f9fa;
            color: #333333;
        }
    )");
    
    QVBoxLayout *mainLayout = new QVBoxLayout();
    setLayout(mainLayout);
    mainLayout->setSpacing(20);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    
    setupHeader();
    setupModelInfo();
    setupComparisonTabs();
    setupConnections();
}

void ComparisonScene::setupHeader()
{
    QHBoxLayout *headerLayout = new QHBoxLayout();
    
    backButton = std::make_unique<QPushButton>("← Назад");
    backButton->setStyleSheet(R"(
        QPushButton {
            background-color: #6c757d;
            color: white;
            border: none;
            border-radius: 6px;
            padding: 10px 20px;
            font-size: 14px;
            font-weight: 500;
        }
        QPushButton:hover {
            background-color: #5a6268;
        }
        QPushButton:pressed {
            background-color: #545b62;
        }
    )");
    headerLayout->addWidget(backButton.get());
    
    headerLayout->addStretch();
    
    titleLabel = std::make_unique<QLabel>("🔍 Сравнение моделей");
    titleLabel->setStyleSheet(R"(
        font-size: 24px; 
        font-weight: 700;
        color: #2c3e50;
        margin: 0;
    )");
    headerLayout->addWidget(titleLabel.get());
    
    headerLayout->addStretch();
    
    saveButton = std::make_unique<QPushButton>("💾 Сохранить");
    saveButton->setStyleSheet(R"(
        QPushButton {
            background-color: #28a745;
            color: white;
            border: none;
            border-radius: 6px;
            padding: 10px 20px;
            font-size: 14px;
            font-weight: 500;
        }
        QPushButton:hover {
            background-color: #218838;
        }
        QPushButton:pressed {
            background-color: #1e7e34;
        }
    )");
    headerLayout->addWidget(saveButton.get());
    
    QVBoxLayout *mainLayout = qobject_cast<QVBoxLayout*>(layout());
    mainLayout->addLayout(headerLayout);
}

void ComparisonScene::setupModelInfo()
{
    QGroupBox *infoGroup = new QGroupBox("📊 Информация о моделях");
    infoGroup->setStyleSheet(R"(
        QGroupBox {
            font-size: 14px;
            font-weight: 600;
            color: #2c3e50;
            border: none;
            margin-top: 5px;
            padding-top: 5px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 5px 0 5px;
        }
    )");
    
    QHBoxLayout *infoLayout = new QHBoxLayout(infoGroup);
    infoLayout->setSpacing(12);
    infoLayout->setContentsMargins(12, 8, 12, 8);
    
    QLabel *pathLabel = new QLabel("Путь к упрощенной модели:");
    pathLabel->setStyleSheet("font-weight: 500; color: #555; font-size: 13px;");
    infoLayout->addWidget(pathLabel);
    
    newModelPathEdit = std::make_unique<QLineEdit>();
    newModelPathEdit->setPlaceholderText("Выберите путь для сохранения упрощенной модели...");
    newModelPathEdit->setStyleSheet(R"(
        QLineEdit {
            border: 1px solid #ced4da;
            border-radius: 4px;
            padding: 6px 10px;
            font-size: 13px;
            background-color: white;
        }
        QLineEdit:focus {
            border-color: #007bff;
        }
    )");
    newModelPathEdit->setMaximumHeight(32);
    infoLayout->addWidget(newModelPathEdit.get(), 2);
    
    browseButton = std::make_unique<QPushButton>("📁 Обзор");
    browseButton->setStyleSheet(R"(
        QPushButton {
            background-color: #17a2b8;
            color: white;
            border: none;
            border-radius: 4px;
            padding: 6px 14px;
            font-size: 13px;
            font-weight: 500;
        }
        QPushButton:hover {
            background-color: #138496;
        }
    )");
    browseButton->setMaximumHeight(32);
    infoLayout->addWidget(browseButton.get());
    
    modelPathLabel = std::make_unique<QLabel>("Загрузите модели для сравнения");
    modelPathLabel->setStyleSheet(R"(
        font-size: 13px;
        color: #6c757d;
        padding: 6px 8px;
        background-color: #f8f9fa;
        border: 1px solid #dee2e6;
        border-radius: 4px;
    )");
    modelPathLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    modelPathLabel->setMaximumHeight(40);
    modelPathLabel->setWordWrap(true);
    infoLayout->addWidget(modelPathLabel.get(), 1);
    
    QVBoxLayout *mainLayout = qobject_cast<QVBoxLayout*>(layout());
    mainLayout->addWidget(infoGroup);
}

void ComparisonScene::setupComparisonTabs()
{
    comparisonTabs = std::make_unique<QTabWidget>();
    comparisonTabs->setStyleSheet(R"(
        QTabWidget::pane {
            border: 1px solid #e0e0e0;
            border-radius: 8px;
            background-color: white;
        }
        QTabBar::tab {
            background-color: #f8f9fa;
            color: #333333;
            border: 1px solid #e0e0e0;
            border-bottom: none;
            border-radius: 6px 6px 0 0;
            padding: 12px 24px;
            margin-right: 2px;
            font-weight: 500;
        }
        QTabBar::tab:selected {
            background-color: white;
            color: #007bff;
            border-bottom: 1px solid white;
        }
        QTabBar::tab:hover {
            background-color: #e9ecef;
        }
    )");
    
    setupTextComparison();
    setupStatisticsCharts();
    setupPolygonComparison();
    setupSimplificationReportTab();
    
    QVBoxLayout *mainLayout = qobject_cast<QVBoxLayout*>(layout());
    mainLayout->addWidget(comparisonTabs.get());
}

void ComparisonScene::setupTextComparison()
{
    QWidget *textTab = new QWidget();
    QVBoxLayout *textLayout = new QVBoxLayout(textTab);
    
    QLabel *comparisonLabel = new QLabel("Сравнение характеристик моделей");
    comparisonLabel->setStyleSheet("font-size: 18px; font-weight: 600; color: #333333; margin-bottom: 16px;");
    textLayout->addWidget(comparisonLabel);
    
    // Добавляем информацию о количестве линий
    if (!originalLines.isEmpty() && !simplifiedLines.isEmpty()) {
        int originalCount = originalLines.size();
        int simplifiedCount = simplifiedLines.size();
        double reductionPercent = (1.0 - (double)simplifiedCount / originalCount) * 100;
        
        QLabel *linesInfoLabel = new QLabel(QString("📊 Линейные функции: %1 → %2 (удалено %3%)")
                                           .arg(originalCount)
                                           .arg(simplifiedCount)
                                           .arg(QString::number(reductionPercent, 'f', 1)));
        linesInfoLabel->setStyleSheet("font-size: 14px; color: #28a745; font-weight: 500; margin-bottom: 16px; padding: 8px; background-color: #d4edda; border-radius: 4px;");
        textLayout->addWidget(linesInfoLabel);
    }
    
    // QHBoxLayout *tablesLayout = new QHBoxLayout(); // Не используется
    
    QHBoxLayout *tablesLayout = new QHBoxLayout();
    tablesLayout->setSpacing(16);
    
    auto createTableWithLabel = [&](const QString &title, const QString &color, std::unique_ptr<QTableWidget> &tablePtr) {
        QWidget *container = new QWidget();
        QVBoxLayout *containerLayout = new QVBoxLayout(container);
        containerLayout->setSpacing(8);
        containerLayout->setContentsMargins(0, 0, 0, 0);
        
        QLabel *label = new QLabel(title);
        label->setStyleSheet(QString("font-size: 16px; font-weight: 600; color: %1;").arg(color));
        containerLayout->addWidget(label);
        
        tablePtr = std::make_unique<QTableWidget>();
        tablePtr->setColumnCount(2);
        tablePtr->setHorizontalHeaderLabels(QStringList() << "Параметр" << "Значение");
        tablePtr->setStyleSheet(R"(
            QTableWidget {
                border: 1px solid #dee2e6;
                border-radius: 6px;
                background-color: white;
                gridline-color: #dee2e6;
            }
            QTableWidget::item {
                padding: 8px;
                border-bottom: 1px solid #f8f9fa;
            }
            QTableWidget::item:selected {
                background-color: #e3f2fd;
            }
            QHeaderView::section {
                background-color: #f8f9fa;
                padding: 8px;
                border: none;
                border-bottom: 2px solid #dee2e6;
                font-weight: 600;
            }
        )");
        tablePtr->horizontalHeader()->setStretchLastSection(true);
        tablePtr->setAlternatingRowColors(true);
        tablePtr->setSelectionBehavior(QAbstractItemView::SelectRows);
        tablePtr->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        
        containerLayout->addWidget(tablePtr.get());
        tablesLayout->addWidget(container);
    };
    
    createTableWithLabel("Оригинальная модель", "#007bff", originalTable);
    createTableWithLabel("Упрощенная модель", "#28a745", simplifiedTable);
    
    textLayout->addLayout(tablesLayout);
    
    comparisonTabs->addTab(textTab, "📋 Текст");
}


void ComparisonScene::setupStatisticsCharts()
{
    QWidget *statsTab = new QWidget();
    QVBoxLayout *statsLayout = new QVBoxLayout(statsTab);
    
    QLabel *statsLabel = new QLabel("Статистика и метрики");
    statsLabel->setStyleSheet("font-size: 18px; font-weight: 600; color: #333333; margin-bottom: 16px;");
    statsLayout->addWidget(statsLabel);
    
    // Создаем виджеты для диаграмм QtCharts
    metricsChartView = std::make_unique<QChartView>();
    parametersChartView = std::make_unique<QChartView>();
    sizeChartView = std::make_unique<QChartView>();
    accuracyChartView = std::make_unique<QChartView>();
    
    // Создаем диаграммы
    createMetricsChart();
    createParametersChart();
    createSizeChart();
    createAccuracyChart();
    
    // Создаем контейнер с прокруткой для диаграмм
    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setMinimumHeight(400);
    scrollArea->setMaximumHeight(800);
    scrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    // Стили для полос прокрутки
    scrollArea->setStyleSheet(R"(
        QScrollArea {
            border: 1px solid #dee2e6;
            border-radius: 8px;
            background-color: white;
        }
        QScrollBar:vertical {
            background-color: #f8f9fa;
            width: 12px;
            border-radius: 6px;
        }
        QScrollBar::handle:vertical {
            background-color: #6c757d;
            border-radius: 6px;
            min-height: 20px;
        }
        QScrollBar::handle:vertical:hover {
            background-color: #495057;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
        QScrollBar:horizontal {
            background-color: #f8f9fa;
            height: 12px;
            border-radius: 6px;
        }
        QScrollBar::handle:horizontal {
            background-color: #6c757d;
            border-radius: 6px;
            min-width: 20px;
        }
        QScrollBar::handle:horizontal:hover {
            background-color: #495057;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            width: 0px;
        }
    )");
    
    // Создаем виджет для диаграмм
    QWidget *chartsWidget = new QWidget();
    QGridLayout *chartsLayout = new QGridLayout(chartsWidget);
    chartsLayout->setSpacing(20);
    chartsLayout->setContentsMargins(20, 20, 20, 20);
    chartsWidget->setMinimumSize(800, 1000); // Устанавливаем минимальный размер для корректной прокрутки
    chartsWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    // Добавляем диаграммы в layout
    chartsLayout->addWidget(metricsChartView.get(), 0, 0);
    chartsLayout->addWidget(parametersChartView.get(), 0, 1);
    chartsLayout->addWidget(sizeChartView.get(), 1, 0);
    chartsLayout->addWidget(accuracyChartView.get(), 1, 1);
    
    // Устанавливаем виджет в прокручиваемую область
    scrollArea->setWidget(chartsWidget);
    
    statsLayout->addWidget(scrollArea);
    
    comparisonTabs->addTab(statsTab, "📈 Статистика");
}

void ComparisonScene::setupSimplificationReportTab()
{
    QWidget *reportTab = new QWidget();
    QVBoxLayout *reportLayout = new QVBoxLayout(reportTab);
    reportLayout->setSpacing(12);
    
    QLabel *reportLabel = new QLabel("Подробный отчет процесса упрощения");
    reportLabel->setStyleSheet("font-size: 18px; font-weight: 600; color: #333333; margin-bottom: 4px;");
    reportLayout->addWidget(reportLabel);
    
    QLabel *reportHint = new QLabel("Каждый этап упрощения фиксируется автоматически. Лог представлен в хронологическом порядке.");
    reportHint->setStyleSheet("font-size: 13px; color: #666666; margin-bottom: 12px;");
    reportHint->setWordWrap(true);
    reportLayout->addWidget(reportHint);
    
    simplificationReportTextEdit = std::make_unique<QTextEdit>();
    simplificationReportTextEdit->setReadOnly(true);
    simplificationReportTextEdit->setMinimumHeight(420);
    simplificationReportTextEdit->setStyleSheet(R"(
        QTextEdit {
            border: 1px solid #dee2e6;
            border-radius: 8px;
            background-color: #ffffff;
            font-family: "Consolas", "Courier New", monospace;
            font-size: 13px;
            color: #343a40;
            padding: 12px;
        }
        QTextEdit:disabled {
            color: #adb5bd;
        }
    )");
    reportLayout->addWidget(simplificationReportTextEdit.get());
    
    reportLayout->addStretch(1);
    
    comparisonTabs->addTab(reportTab, "📝 Отчет упрощения");
    updateSimplificationReportView();
}

void ComparisonScene::setupPolygonComparison()
{
    QWidget *polygonTab = new QWidget();
    QHBoxLayout *mainContentLayout = new QHBoxLayout(polygonTab);
    mainContentLayout->setSpacing(20);
    mainContentLayout->setContentsMargins(16, 16, 16, 16);
    
    // ========== LEFT PANEL: CONTROL PANEL ==========
    QWidget *controlPanel = new QWidget();
    controlPanel->setMaximumWidth(400);
    controlPanel->setMinimumWidth(370);
    QVBoxLayout *controlLayout = new QVBoxLayout(controlPanel);
    controlLayout->setSpacing(16);
    controlLayout->setContentsMargins(16, 16, 16, 16);
    controlPanel->setStyleSheet(R"(
        QWidget {
            background-color: #f8f9fa;
            border: none;
            border-radius: 0px;
        }
    )");
    
    // Заголовок блока управления
    QLabel *controlTitle = new QLabel("⚙️ Управление сравнением");
    controlTitle->setStyleSheet("font-size: 18px; font-weight: 600; color: #333333; margin-bottom: 8px;");
    controlLayout->addWidget(controlTitle);
    
    // Подпись сравнения
    comparisonLabel = std::make_unique<QLabel>("Сравнение оригинальной и упрощенной модели");
    comparisonLabel->setStyleSheet("font-size: 13px; color: #666666; padding: 8px 0;");
    comparisonLabel->setAlignment(Qt::AlignCenter);
    comparisonLabel->setWordWrap(true);
    comparisonLabel->setMinimumHeight(50);  // Минимальная высота для текста
    controlLayout->addWidget(comparisonLabel.get());

    // Разделитель
    QFrame *separator1 = new QFrame();
    separator1->setFrameShape(QFrame::HLine);
    separator1->setFrameShadow(QFrame::Sunken);
    separator1->setStyleSheet("color: #e0e0e0;");
    controlLayout->addWidget(separator1);
    
    // Кнопки управления режимами отображения
    QLabel *viewModeLabel = new QLabel("👁️ Режим отображения:");
    viewModeLabel->setStyleSheet("font-size: 14px; font-weight: 600; color: #333333; margin-top: 12px;");
    controlLayout->addWidget(viewModeLabel);
    
    viewOriginalButton = new QPushButton("📊 Только оригинал");
    viewOriginalButton->setCheckable(true);
    viewOriginalButton->setStyleSheet(R"(
        QPushButton {
            background-color: #3498db;
            color: white;
            border: none;
            border-radius: 6px;
            padding: 10px 16px;
            font-size: 13px;
            font-weight: 500;
            text-align: left;
            margin-top: 6px;
        }
        QPushButton:checked {
            background-color: #2980b9;
        }
        QPushButton:hover:not(:checked) {
            background-color: #2980b9;
        }
    )");
    viewOriginalButton->setChecked(true);
    controlLayout->addWidget(viewOriginalButton);
    
    viewSimplifiedButton = new QPushButton("📊 Только упрощенный");
    viewSimplifiedButton->setCheckable(true);
    viewSimplifiedButton->setStyleSheet(R"(
        QPushButton {
            background-color: #e67e22;
            color: white;
            border: none;
            border-radius: 6px;
            padding: 10px 16px;
            font-size: 13px;
            font-weight: 500;
            text-align: left;
            margin-top: 6px;
        }
        QPushButton:checked {
            background-color: #d35400;
        }
        QPushButton:hover:not(:checked) {
            background-color: #d35400;
        }
    )");
    viewSimplifiedButton->setChecked(false);
    controlLayout->addWidget(viewSimplifiedButton);
    
    // Подключаем кнопки
    connect(viewOriginalButton, &QPushButton::clicked, this, [this]() {
        switchViewMode(0);
    });
    connect(viewSimplifiedButton, &QPushButton::clicked, this, [this]() {
        switchViewMode(1);
    });
    
    // Разделитель
    QFrame *separator2 = new QFrame();
    separator2->setFrameShape(QFrame::HLine);
    separator2->setFrameShadow(QFrame::Sunken);
    separator2->setStyleSheet("color: #e0e0e0; margin-top: 12px;");
    controlLayout->addWidget(separator2);
    
    controlLayout->addStretch();
    
    mainContentLayout->addWidget(controlPanel);
    
    // ========== RIGHT PANEL: STACKED WIDGET FOR DIFFERENT VIEW MODES ==========
    polygonStackedWidget = new QStackedWidget();
    polygonStackedWidget->setStyleSheet(R"(
        QStackedWidget {
            background-color: #ffffff;
            border: none;
        }
    )");
    
    // Добавляем режимы отображения: 0 — оригинал, 1 — упрощенная модель
    QWidget *originalContainer = createSingleGraphWidget(true);  // true = оригинальный
    QWidget *simplifiedContainer = createSingleGraphWidget(false);  // false = упрощенный
    polygonStackedWidget->addWidget(originalContainer);
    polygonStackedWidget->addWidget(simplifiedContainer);
    polygonStackedWidget->setCurrentIndex(0);
    
    mainContentLayout->addWidget(polygonStackedWidget, 2);  // Графики занимают больше места
    
    comparisonTabs->addTab(polygonTab, "📊 Полигоны");
}

// Создание виджета для одного графика (оригинального или упрощенного)
QWidget* ComparisonScene::createSingleGraphWidget(bool isOriginal)
{
    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setStyleSheet(R"(
        QScrollArea {
            border: none;
            background-color: #ffffff;
        }
        QScrollBar:vertical {
            background: #f0f0f0;
            width: 12px;
            border-radius: 0px;
        }
        QScrollBar::handle:vertical {
            background: #c0c0c0;
            min-height: 20px;
            border-radius: 0px;
        }
        QScrollBar::handle:vertical:hover {
            background: #a0a0a0;
        }
        QScrollBar:horizontal {
            background: #f0f0f0;
            height: 12px;
            border-radius: 0px;
        }
        QScrollBar::handle:horizontal {
            background: #c0c0c0;
            min-width: 20px;
            border-radius: 0px;
        }
        QScrollBar::handle:horizontal:hover {
            background: #a0a0a0;
        }
    )");
    
    QWidget *graphWidget = new QWidget();
    QVBoxLayout *graphLayout = new QVBoxLayout(graphWidget);
    graphLayout->setSpacing(8);
    graphLayout->setContentsMargins(8, 8, 8, 8);
    
    QString title = isOriginal ? "📊 Оригинальная модель" : "📊 Упрощенная модель";
    QLabel *titleLabel = new QLabel(title);
    titleLabel->setStyleSheet(R"(
        font-size: 16px; 
        font-weight: 600; 
        color: #333333;
        padding: 4px 0;
        margin-bottom: 4px;
    )");
    graphLayout->addWidget(titleLabel);
    
    // Создаем виджет полигонов
    NeuralNetworkPolygonWidget *polygonWidget;
    if (isOriginal) {
        originalPolygonWidget = std::make_unique<NeuralNetworkPolygonWidget>();
        polygonWidget = originalPolygonWidget.get();
        polygonWidget->setStyleSheet(R"(
            QWidget {
                border: 2px solid #3498db;
                border-radius: 0px;
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, 
                    stop:0 #f8f9fa, stop:1 #e9ecef);
            }
        )");
    } else {
        simplifiedPolygonWidget = std::make_unique<NeuralNetworkPolygonWidget>();
        polygonWidget = simplifiedPolygonWidget.get();
        polygonWidget->setStyleSheet(R"(
            QWidget {
                border: 2px solid #e67e22;
                border-radius: 0px;
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, 
                    stop:0 #f9f6f2, stop:1 #f2e8da);
            }
        )");
    }
    
    polygonWidget->setMinimumSize(500, 500);
    graphLayout->addWidget(polygonWidget, 1);
    
    // Блок управления
    QWidget *controlWidget = new QWidget();
    QVBoxLayout *controlLayout = new QVBoxLayout(controlWidget);
    controlLayout->setSpacing(8);
    controlLayout->setContentsMargins(8, 8, 8, 8);
    
    QString layerLabelText = isOriginal ? "📊 Слой (оригинал):" : "📊 Слой (упрощенный):";
    QLabel *layerLabel = new QLabel(layerLabelText);
    layerLabel->setStyleSheet("font-size: 13px; font-weight: 500; color: #333333;");
    controlLayout->addWidget(layerLabel);
    
    QComboBox *layerComboBox = new QComboBox();
    layerComboBox->addItem("Все слои", -1);
    QString borderColor = isOriginal ? "#3498db" : "#e67e22";
    QString hoverColor = isOriginal ? "#2980b9" : "#d35400";
    layerComboBox->setStyleSheet(QString(R"(
        QComboBox {
            background-color: #ffffff;
            border: 1px solid %1;
            border-radius: 0px;
            padding: 5px 10px;
            font-size: 12px;
        }
        QComboBox:hover {
            border-color: %2;
        }
    )").arg(borderColor).arg(hoverColor));
    controlLayout->addWidget(layerComboBox);
    
    // Сохраняем указатели
    if (isOriginal) {
        originalLayerComboBoxPtr = layerComboBox;
        connect(layerComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, layerComboBox](int index) {
            int layerIndex = layerComboBox->itemData(index).toInt();
            if (originalPolygonWidget) {
                originalPolygonWidget->setSelectedLayer(layerIndex);
                originalPolygonWidget->update();
            }
        });
    } else {
        simplifiedLayerComboBoxPtr = layerComboBox;
        connect(layerComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, layerComboBox](int index) {
            int layerIndex = layerComboBox->itemData(index).toInt();
            if (simplifiedPolygonWidget) {
                simplifiedPolygonWidget->setSelectedLayer(layerIndex);
                simplifiedPolygonWidget->update();
            }
        });
    }
    
    graphLayout->insertWidget(1, controlWidget);
    
    graphWidget->setLayout(graphLayout);
    scrollArea->setWidget(graphWidget);
    
    return scrollArea;
}

// Переключение режима отображения
void ComparisonScene::switchViewMode(int mode)
{
    if (mode == currentViewMode) return;
    
    currentViewMode = mode;
    
    // Обновляем состояние кнопок
    if (viewOriginalButton) {
        viewOriginalButton->setChecked(mode == 0);
    }
    if (viewSimplifiedButton) {
        viewSimplifiedButton->setChecked(mode == 1);
    }
    
    // Переключаем виджет
    if (polygonStackedWidget) {
        polygonStackedWidget->setCurrentIndex(mode);
    }
    
    // Обновляем отображение
    if (mode == 0 && originalPolygonWidget) {
        originalPolygonWidget->update();
    } else if (mode == 1 && simplifiedPolygonWidget) {
        simplifiedPolygonWidget->update();
    }
}

// Обновление полигонов при изменении данных
void ComparisonScene::updatePolygonComparison()
{
    qDebug() << "ComparisonScene::updatePolygonComparison - обновление полигонов:";
    qDebug() << "  - originalPolygonWidget:" << (originalPolygonWidget != nullptr);
    qDebug() << "  - simplifiedPolygonWidget:" << (simplifiedPolygonWidget != nullptr);
    qDebug() << "  - originalLines размер:" << originalLines.size();
    qDebug() << "  - simplifiedLines размер:" << simplifiedLines.size();
    
    auto collectLayers = [](const QVector<LinearFunction> &lines) {
        std::set<int> layers;
        for (const auto &line : lines) {
            layers.insert(line.layer);
        }
        return layers;
    };
    
    auto populateLayerCombo = [](QComboBox *combo, const std::set<int> &layers) {
        if (!combo) {
            return;
        }
        QSignalBlocker blocker(combo);
        int currentLayer = combo->currentData().toInt();
        combo->clear();
        combo->addItem("Все слои", -1);
        for (int layer : layers) {
            combo->addItem(QString("Слой %1").arg(layer + 1), layer);
        }
        int targetIndex = combo->findData(currentLayer);
        combo->setCurrentIndex(targetIndex >= 0 ? targetIndex : 0);
    };
    
    // Обновляем оригинальный виджет
    if (originalPolygonWidget) {
        originalPolygonWidget->setLinearFunctions(originalLines);
        originalPolygonWidget->setDecisionPolygon(originalPolygon);
        populateLayerCombo(originalLayerComboBoxPtr, collectLayers(originalLines));
        if (originalLayerComboBoxPtr) {
            originalPolygonWidget->setSelectedLayer(originalLayerComboBoxPtr->currentData().toInt());
        }
        originalPolygonWidget->update();
    }
    
    // Обновляем упрощенный виджет
    if (simplifiedPolygonWidget) {
        simplifiedPolygonWidget->setLinearFunctions(simplifiedLines);
        simplifiedPolygonWidget->setDecisionPolygon(simplifiedPolygon);
        populateLayerCombo(simplifiedLayerComboBoxPtr, collectLayers(simplifiedLines));
        if (simplifiedLayerComboBoxPtr) {
            simplifiedPolygonWidget->setSelectedLayer(simplifiedLayerComboBoxPtr->currentData().toInt());
        }
        simplifiedPolygonWidget->update();
    }
    
    // Обновляем совмещенный виджет
    
    // Обновляем метрики сравнения
    updateComparisonMetrics();
}

void ComparisonScene::setupConnections()
{
    connect(backButton.get(), &QPushButton::clicked, this, [this]() {
        onBackClicked();
    });
    connect(saveButton.get(), &QPushButton::clicked, this, [this]() {
        onSaveClicked();
    });
    connect(browseButton.get(), &QPushButton::clicked, this, [this]() {
        onBrowseClicked();
    });
    connect(comparisonTabs.get(), &QTabWidget::currentChanged, this, [this](int index) {
        onTabChanged(index);
    });
}

void ComparisonScene::setModels(const QJsonObject &originalModel, const QJsonObject &simplifiedModel, const QJsonObject &result)
{
    originalModelData = originalModel;
    simplifiedModelData = simplifiedModel;
    simplificationResult = result;
    
    updateModelInfo();
    populateTextComparison();
    populateStatisticsCharts();
    updateSimplificationReportView();
}

void ComparisonScene::setPolygonData(const QVector<LinearFunction> &originalLines, const QVector<LinearFunction> &simplifiedLines,
                                   const DecisionPolygon &originalPolygon, const DecisionPolygon &simplifiedPolygon)
{
    qDebug() << "ComparisonScene::setPolygonData - получены данные:";
    qDebug() << "  - originalLines размер:" << originalLines.size();
    qDebug() << "  - simplifiedLines размер:" << simplifiedLines.size();
    qDebug() << "  - originalPolygon вершин:" << originalPolygon.vertices.size();
    qDebug() << "  - simplifiedPolygon вершин:" << simplifiedPolygon.vertices.size();
    
    this->originalLines = originalLines;
    this->originalPolygon = originalPolygon;
    this->simplifiedPolygon = simplifiedPolygon;
    
    // КРИТИЧЕСКАЯ ПРОВЕРКА: упрощенная модель должна иметь меньше линий!
    // ЛОГИРОВАНИЕ/ASSERT вместо автоматического обрезания - чтобы не скрывать баг
    if (simplifiedLines.size() >= originalLines.size()) {
        qDebug() << "❌❌❌ КРИТИЧЕСКАЯ ОШИБКА АЛГОРИТМА УПРОЩЕНИЯ ❌❌❌";
        qDebug() << "  - Оригинальных линий:" << originalLines.size();
        qDebug() << "  - Упрощенных линий:" << simplifiedLines.size();
        qDebug() << "  - Разница:" << (simplifiedLines.size() - originalLines.size());
        qDebug() << "  - ⚠️ Упрощенная модель имеет больше или столько же линий!";
        qDebug() << "  - ⚠️ Это указывает на БАГ в алгоритме упрощения!";
        qDebug() << "  - ⚠️ Пожалуйста, исправьте алгоритм в performRealSimplification()";
        
        // Выводим диалог с предупреждением
        QMessageBox::warning(nullptr, 
                            "Ошибка алгоритма упрощения",
                            QString("КРИТИЧЕСКАЯ ОШИБКА: Упрощенная модель имеет %1 линий,\n"
                                   "что больше или равно оригинальной модели (%2 линий).\n\n"
                                   "Это указывает на проблему в алгоритме упрощения.\n\n"
                                   "Проверьте логи для подробностей.")
                            .arg(simplifiedLines.size())
                            .arg(originalLines.size()));
        
        // НЕ обрезаем автоматически - это скрыло бы баг!
        // Используем исходные данные и оставляем проблему видимой
    }
    
    // Используем исходные данные (не обрезаем автоматически)
    this->simplifiedLines = simplifiedLines;
    
    // Обновляем виджеты с данными
    updatePolygonComparison();
}

void ComparisonScene::updateModelInfo()
{
    if (originalModelData.isEmpty() || simplifiedModelData.isEmpty()) {
        modelPathLabel->setText("Модели не загружены");
        return;
    }
    
    // Получаем данные из JSON (используем правильные имена полей)
    int originalLayers = originalModelData["layers"].toArray().size();
    int originalParams = originalModelData["total_params"].toInt();
    int simplifiedLayers = simplifiedModelData["layers"].toArray().size();
    int simplifiedParams = simplifiedModelData["total_params"].toInt();
    
    QString info = QString("Оригинальная модель: %1 слоев, %2 параметров\nУпрощенная модель: %3 слоев, %4 параметров")
                   .arg(originalLayers)
                   .arg(originalParams)
                   .arg(simplifiedLayers)
                   .arg(simplifiedParams);
    
    modelPathLabel->setText(info);
}

void ComparisonScene::populateTextComparison()
{
    if (originalTable && simplifiedTable) {
        // Заполняем таблицы данными моделей
    QStringList parameters = {"Слои", "Параметры", "Размер (MB)", "Точность"};
        
        originalTable->setRowCount(parameters.size());
        simplifiedTable->setRowCount(parameters.size());
        
        // Данные для оригинальной модели
        QStringList originalValues;
        if (!originalModelData.isEmpty()) {
            QJsonArray layers = originalModelData["layers"].toArray();
            QJsonValue originalAccuracyValue = originalModelData.value("accuracy");
            QString originalAccuracyText = originalAccuracyValue.isDouble()
                ? QStringLiteral("%1%").arg(QString::number(originalAccuracyValue.toDouble() * 100.0, 'f', 2))
                : QStringLiteral("—");

            originalValues << QString::number(layers.size());
            originalValues << QString::number(originalModelData["total_params"].toInt());
            originalValues << QString::number(originalModelData["model_size_mb"].toDouble(), 'f', 2);
            originalValues << originalAccuracyText;
        } else {
            originalValues = {"N/A", "N/A", "N/A", "—"};
        }
        
        // Данные для упрощенной модели
        QStringList simplifiedValues;
        if (!simplifiedModelData.isEmpty()) {
            QJsonArray layers = simplifiedModelData["layers"].toArray();
            QJsonValue simplifiedAccuracyValue = simplifiedModelData.value("accuracy");
            QString simplifiedAccuracyText = simplifiedAccuracyValue.isDouble()
                ? QStringLiteral("%1%").arg(QString::number(simplifiedAccuracyValue.toDouble() * 100.0, 'f', 2))
                : QStringLiteral("—");

            simplifiedValues << QString::number(layers.size());
            simplifiedValues << QString::number(simplifiedModelData["total_params"].toInt());
            simplifiedValues << QString::number(simplifiedModelData["model_size_mb"].toDouble(), 'f', 2);
            simplifiedValues << simplifiedAccuracyText;
        } else {
            simplifiedValues = {"N/A", "N/A", "N/A", "—"};
        }
        
        for (int i = 0; i < parameters.size(); ++i) {
            originalTable->setItem(i, 0, new QTableWidgetItem(parameters[i]));
            originalTable->setItem(i, 1, new QTableWidgetItem(originalValues[i]));
            
            simplifiedTable->setItem(i, 0, new QTableWidgetItem(parameters[i]));
            simplifiedTable->setItem(i, 1, new QTableWidgetItem(simplifiedValues[i]));
        }
    }
}

void ComparisonScene::populateStatisticsCharts()
{
    qDebug() << "ComparisonScene::populateStatisticsCharts - заполнение метрик:";
    qDebug() << "  - originalModelData пуста:" << originalModelData.isEmpty();
    qDebug() << "  - simplifiedModelData пуста:" << simplifiedModelData.isEmpty();
    
    if (originalModelData.isEmpty() || simplifiedModelData.isEmpty()) {
        qDebug() << "  - Данные моделей пусты, метрики не заполняются";
        return;
    }
    
    const bool hasSummary = !simplificationResult.isEmpty();
    
    // Отладочная информация о точности
    qDebug() << "  - Проверка точности в originalModelData:";
    if (originalModelData.contains("accuracy")) {
        QJsonValue accVal = originalModelData.value("accuracy");
        qDebug() << "    - Ключ 'accuracy' найден, тип:" << accVal.type() << "значение:" << accVal.toVariant();
        if (accVal.isDouble()) {
            qDebug() << "    - ✓ Точность оригинальной модели:" << accVal.toDouble() << "(" << (accVal.toDouble() * 100.0) << "%)";
        } else if (accVal.isNull()) {
            qDebug() << "    - ⚠️ Точность оригинальной модели: null";
        }
    } else {
        qDebug() << "    - ⚠️ Ключ 'accuracy' отсутствует в originalModelData";
        qDebug() << "    - Доступные ключи:" << originalModelData.keys();
    }
    
    qDebug() << "  - Проверка точности в simplifiedModelData:";
    if (simplifiedModelData.contains("accuracy")) {
        QJsonValue accVal = simplifiedModelData.value("accuracy");
        qDebug() << "    - Ключ 'accuracy' найден, тип:" << accVal.type() << "значение:" << accVal.toVariant();
        if (accVal.isDouble()) {
            qDebug() << "    - ✓ Точность упрощенной модели:" << accVal.toDouble() << "(" << (accVal.toDouble() * 100.0) << "%)";
        } else if (accVal.isNull()) {
            qDebug() << "    - ⚠️ Точность упрощенной модели: null";
        }
    } else {
        qDebug() << "    - ⚠️ Ключ 'accuracy' отсутствует в simplifiedModelData";
        qDebug() << "    - Доступные ключи:" << simplifiedModelData.keys();
    }
    
    // Получаем данные из summary, если они есть
    int originalParams = hasSummary
        ? simplificationResult.value("originalParams").toInt(originalModelData["total_params"].toInt())
        : originalModelData["total_params"].toInt();
    int simplifiedParams = hasSummary
        ? simplificationResult.value("simplifiedParams").toInt(simplifiedModelData["total_params"].toInt())
        : simplifiedModelData["total_params"].toInt();
    double originalSize = hasSummary
        ? simplificationResult.value("originalSizeMb").toDouble(originalModelData["model_size_mb"].toDouble())
        : originalModelData["model_size_mb"].toDouble();
    double simplifiedSize = hasSummary
        ? simplificationResult.value("simplifiedSizeMb").toDouble(simplifiedModelData["model_size_mb"].toDouble())
        : simplifiedModelData["model_size_mb"].toDouble();
    
    // Приоритет: сначала summary, потом из самих моделей
    QJsonValue originalAccuracyValue;
    QJsonValue simplifiedAccuracyValue;
    
    if (hasSummary) {
        originalAccuracyValue = simplificationResult.value("originalAccuracy");
        simplifiedAccuracyValue = simplificationResult.value("simplifiedAccuracy");
        qDebug() << "  - Используем точность из summary:";
        qDebug() << "    - originalAccuracy из summary:" << originalAccuracyValue.toVariant();
        qDebug() << "    - simplifiedAccuracy из summary:" << simplifiedAccuracyValue.toVariant();
    }
    
    // Если в summary нет, берем из самих моделей
    if (!originalAccuracyValue.isDouble() && originalModelData.contains("accuracy")) {
        originalAccuracyValue = originalModelData.value("accuracy");
        qDebug() << "  - Используем точность из originalModelData:" << originalAccuracyValue.toVariant();
    }
    
    if (!simplifiedAccuracyValue.isDouble() && simplifiedModelData.contains("accuracy")) {
        simplifiedAccuracyValue = simplifiedModelData.value("accuracy");
        qDebug() << "  - Используем точность из simplifiedModelData:" << simplifiedAccuracyValue.toVariant();
    }
    
    bool hasOriginalAccuracy = originalAccuracyValue.isDouble();
    bool hasSimplifiedAccuracy = simplifiedAccuracyValue.isDouble();
    double originalAccuracy = hasOriginalAccuracy ? originalAccuracyValue.toDouble() : 0.0;
    double simplifiedAccuracy = hasSimplifiedAccuracy ? simplifiedAccuracyValue.toDouble() : 0.0;
    
    qDebug() << "  - originalParams:" << originalParams << "simplifiedParams:" << simplifiedParams;
    qDebug() << "  - originalSize:" << originalSize << "simplifiedSize:" << simplifiedSize;
    qDebug() << "  - originalAccuracy available:" << hasOriginalAccuracy << "simplifiedAccuracy available:" << hasSimplifiedAccuracy;
    
    // Вычисляем метрики
    double compressionRatio = hasSummary
        ? simplificationResult.value("compressionRatio").toDouble(
              originalParams > 0 ? 1.0 - static_cast<double>(simplifiedParams) / originalParams : 0.0)
        : (originalParams > 0
              ? 1.0 - static_cast<double>(simplifiedParams) / originalParams
              : 0.0);
    double sizeReduction = hasSummary
        ? simplificationResult.value("sizeReduction").toDouble(
              originalSize > 0.0 ? 1.0 - simplifiedSize / originalSize : 0.0)
        : (originalSize > 0.0
              ? 1.0 - simplifiedSize / originalSize
              : 0.0);
    double accuracyLoss = (hasOriginalAccuracy && hasSimplifiedAccuracy)
        ? qMax(0.0, originalAccuracy - simplifiedAccuracy)
        : 0.0;
    
    // Обновляем диаграммы
    updateMetricsChart(compressionRatio, accuracyLoss);
    updateParametersChart(originalParams, simplifiedParams, compressionRatio);
    updateSizeChart(originalSize, simplifiedSize, sizeReduction);
    if (hasOriginalAccuracy && hasSimplifiedAccuracy) {
        updateAccuracyChart(originalAccuracy, simplifiedAccuracy, accuracyLoss);
    }
    
    qDebug() << "  - Метрики заполнены: compressionRatio=" << compressionRatio 
             << " sizeReduction=" << sizeReduction << " accuracyLoss=" << accuracyLoss;
}

void ComparisonScene::createAccuracyChart()
{
    // Создаем столбчатую диаграмму для точности
    QBarSet *originalSet = new QBarSet("Оригинал");
    QBarSet *simplifiedSet = new QBarSet("Упрощенная");
    
    *originalSet << 0.0;
    *simplifiedSet << 0.0;
    
    QBarSeries *series = new QBarSeries();
    series->append(originalSet);
    series->append(simplifiedSet);
    
    // Настройка цветов столбцов
    originalSet->setColor(QColor(52, 152, 219));  // Синий
    simplifiedSet->setColor(QColor(230, 126, 34));  // Оранжевый
    
    // Создаем диаграмму
    QChart *chart = new QChart();
    chart->addSeries(series);
    chart->setTitle("🎯 Точность модели");
    chart->setTitleFont(QFont("Arial", 16, QFont::Bold));
    chart->setAnimationOptions(QChart::SeriesAnimations);
    
    // Настройка категорийной оси X
    QBarCategoryAxis *axisX = new QBarCategoryAxis();
    axisX->append("Модели");
    axisX->setTitleText("Модели");
    axisX->setTitleFont(QFont("Arial", 12));
    axisX->setLabelsFont(QFont("Arial", 10));
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);
    
    // Настройка числовой оси Y
    QValueAxis *axisY = new QValueAxis();
    axisY->setRange(85, 100);
    axisY->setTitleText("Точность (%)");
    axisY->setTitleFont(QFont("Arial", 12));
    axisY->setLabelsFont(QFont("Arial", 10));
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);
    
    // Настройка виджета
    accuracyChartView->setChart(chart);
    accuracyChartView->setRenderHint(QPainter::Antialiasing);
    accuracyChartView->setMinimumHeight(300);
    accuracyChartView->setMaximumHeight(400);
    accuracyChartView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void ComparisonScene::updateMetricsCard(QWidget *card, const QString &title, const QString &data)
{
    if (!card) return;
    
    // Очищаем существующий контент
    QLayout *layout = card->layout();
    if (layout) {
        QLayoutItem *item;
        while ((item = layout->takeAt(0)) != nullptr) {
            delete item->widget();
            delete item;
        }
    } else {
        layout = new QVBoxLayout(card);
    }
    
    // Создаем заголовок
    QLabel *titleLabel = new QLabel(title);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet(R"(
        QLabel {
            font-size: 16px;
            font-weight: 600;
            color: #333;
            margin-bottom: 8px;
        }
    )");
    
    // Создаем данные
    QLabel *dataLabel = new QLabel(data);
    dataLabel->setAlignment(Qt::AlignCenter);
    dataLabel->setStyleSheet(R"(
        QLabel {
            font-size: 14px;
            color: #666;
            line-height: 1.4;
        }
    )");
    
    layout->addWidget(titleLabel);
    layout->addWidget(dataLabel);
}

void ComparisonScene::updateMetricsChart(double compressionRatio, double accuracyLoss)
{
    if (!metricsChartView) return;
    
    // Обновляем круговую диаграмму с реальными данными
    double compressionPercent = qBound(0.0, compressionRatio * 100.0, 100.0);
    double lossPercent = qBound(0.0, accuracyLoss * 100.0, 100.0);
    double remainderPercent = qMax(0.0, 100.0 - compressionPercent - lossPercent);
    
    QPieSeries *series = new QPieSeries();
    series->append("Сжатие", compressionPercent);
    series->append("Потеря точности", lossPercent);
    series->append("Остальное", remainderPercent);
    
    // Настройка цветов
    series->slices().at(0)->setColor(QColor("#28a745")); // Зеленый для сжатия
    series->slices().at(1)->setColor(QColor("#dc3545")); // Красный для потери
    series->slices().at(2)->setColor(QColor("#6c757d")); // Серый для остального
    
    // Обновляем диаграмму
    QChart *chart = metricsChartView->chart();
    if (chart) {
        chart->removeAllSeries();
        chart->addSeries(series);
    }
}

void ComparisonScene::updateParametersChart(int originalParams, int simplifiedParams, double compressionRatio)
{
    Q_UNUSED(compressionRatio);
    if (!parametersChartView) return;
    
    // Обновляем столбчатую диаграмму с реальными данными
    QBarSet *originalSet = new QBarSet("Оригинал");
    QBarSet *simplifiedSet = new QBarSet("Упрощенная");
    
    *originalSet << originalParams;
    *simplifiedSet << simplifiedParams;
    
    QBarSeries *series = new QBarSeries();
    series->append(originalSet);
    series->append(simplifiedSet);
    
    // Настройка цветов
    originalSet->setColor(QColor("#007bff"));
    simplifiedSet->setColor(QColor("#28a745"));
    
    // Обновляем диаграмму
    QChart *chart = parametersChartView->chart();
    if (chart) {
        chart->removeAllSeries();
        chart->addSeries(series);
        
        // Обновляем ось Y
        QValueAxis *axisY = qobject_cast<QValueAxis*>(chart->axes(Qt::Vertical).first());
        if (axisY) {
            axisY->setRange(0, qMax(originalParams, simplifiedParams) * 1.1);
        }
    }
}

void ComparisonScene::updateSizeChart(double originalSize, double simplifiedSize, double sizeReduction)
{
    Q_UNUSED(sizeReduction);
    if (!sizeChartView) return;
    
    // Обновляем столбчатую диаграмму с реальными данными
    QBarSet *originalSet = new QBarSet("Оригинал");
    QBarSet *simplifiedSet = new QBarSet("Упрощенная");
    
    *originalSet << originalSize;
    *simplifiedSet << simplifiedSize;
    
    QBarSeries *series = new QBarSeries();
    series->append(originalSet);
    series->append(simplifiedSet);
    
    // Настройка цветов
    originalSet->setColor(QColor("#ffc107"));
    simplifiedSet->setColor(QColor("#17a2b8"));
    
    // Обновляем диаграмму
    QChart *chart = sizeChartView->chart();
    if (chart) {
        chart->removeAllSeries();
        chart->addSeries(series);
        
        // Обновляем ось Y
        QValueAxis *axisY = qobject_cast<QValueAxis*>(chart->axes(Qt::Vertical).first());
        if (axisY) {
            axisY->setRange(0, qMax(originalSize, simplifiedSize) * 1.2);
        }
    }
}

void ComparisonScene::updateAccuracyChart(double originalAccuracy, double simplifiedAccuracy, double accuracyLoss)
{
    Q_UNUSED(accuracyLoss);
    if (!accuracyChartView) return;
    
    // Обновляем столбчатую диаграмму с реальными данными
    double origVal = originalAccuracy * 100;
    double simplVal = simplifiedAccuracy * 100;
    
    QBarSet *originalSet = new QBarSet("Оригинал");
    QBarSet *simplifiedSet = new QBarSet("Упрощенная");
    
    *originalSet << origVal;
    *simplifiedSet << simplVal;
    
    // Настройка цветов столбцов
    originalSet->setColor(QColor(52, 152, 219));  // Синий
    simplifiedSet->setColor(QColor(230, 126, 34));  // Оранжевый
    
    QBarSeries *series = new QBarSeries();
    series->append(originalSet);
    series->append(simplifiedSet);
    
    // Обновляем диаграмму
    QChart *chart = accuracyChartView->chart();
    if (!chart) {
        qDebug() << "⚠️ updateAccuracyChart: chart не существует!";
        return;
    }
    
    // Удаляем старые сериусы
    chart->removeAllSeries();
    
    // Добавляем новый сериус
    chart->addSeries(series);
    
    // Получаем или создаем оси
    QBarCategoryAxis *axisX = qobject_cast<QBarCategoryAxis*>(chart->axes(Qt::Horizontal).first());
    QValueAxis *axisY = qobject_cast<QValueAxis*>(chart->axes(Qt::Vertical).first());
    
    // Если осей нет, создаем их (на случай если график еще не инициализирован)
    if (!axisX) {
        axisX = new QBarCategoryAxis();
        axisX->setTitleText("Модели");
        axisX->setTitleFont(QFont("Arial", 12));
        axisX->setLabelsFont(QFont("Arial", 10));
        chart->addAxis(axisX, Qt::AlignBottom);
    }
    
    // Обновляем категории оси X
    QStringList categories;
    categories << "Модели";
    axisX->clear();
    axisX->append(categories);
    
    if (!axisY) {
        axisY = new QValueAxis();
        axisY->setTitleText("Точность (%)");
        axisY->setTitleFont(QFont("Arial", 12));
        axisY->setLabelsFont(QFont("Arial", 10));
        chart->addAxis(axisY, Qt::AlignLeft);
    }
    
    // Привязываем сериус к осям
    series->attachAxis(axisX);
    series->attachAxis(axisY);
    
    // Обновляем диапазон оси Y
    double minVal = qMin(origVal, simplVal) - 5;
    double maxVal = qMax(origVal, simplVal) + 5;
    axisY->setRange(qMax(0.0, minVal), qMin(100.0, maxVal));
    axisY->setTitleText("Точность (%)");
    
    qDebug() << "updateAccuracyChart: обновлен график - оригинал:" << origVal 
             << "%, упрощенная:" << simplVal << "%";
    
    // Принудительное обновление отображения
    chart->update();
    accuracyChartView->update();
}


void ComparisonScene::setTrainingDataPoints(const QVector<QPointF> &points)
{
    qDebug() << "ComparisonScene::setTrainingDataPoints - установка точек данных:" << points.size();
    
    // Передаем точки обучающих данных во все виджеты
    if (originalPolygonWidget) {
        originalPolygonWidget->setTrainingDataPoints(points);
        qDebug() << "  - Точки переданы в originalPolygonWidget";
    }
    if (simplifiedPolygonWidget) {
        simplifiedPolygonWidget->setTrainingDataPoints(points);
        qDebug() << "  - Точки переданы в simplifiedPolygonWidget";
    }
}

void ComparisonScene::setSimplificationReport(const QStringList &report)
{
    simplificationReportLines = report;
    updateSimplificationReportView();
}

void ComparisonScene::updateSimplificationReportView()
{
    if (!simplificationReportTextEdit) {
        return;
    }
    
    if (simplificationReportLines.isEmpty()) {
        simplificationReportTextEdit->setPlainText("Отчет о процессе упрощения пока не сформирован. Выполните упрощение модели, чтобы увидеть подробный лог.");
        simplificationReportTextEdit->moveCursor(QTextCursor::Start);
        return;
    }
    
    simplificationReportTextEdit->setPlainText(simplificationReportLines.join("\n"));
    simplificationReportTextEdit->moveCursor(QTextCursor::Start);
}

void ComparisonScene::updateComparisonMetrics()
{
    qDebug() << "ComparisonScene::updateComparisonMetrics - обновление метрик:";
    qDebug() << "  - originalLines размер:" << originalLines.size();
    qDebug() << "  - simplifiedLines размер:" << simplifiedLines.size();
    qDebug() << "  - comparisonLabel:" << (comparisonLabel != nullptr);
    
    if (originalLines.isEmpty() || simplifiedLines.isEmpty()) {
        qDebug() << "  - Одна из коллекций линий пуста, метрики не обновляются";
        return;
    }
    
    const bool hasSummary = !simplificationResult.isEmpty();
    
    // Используем метрики из summary, если они доступны
    const int originalLineCount = hasSummary
        ? simplificationResult.value("originalLineCount").toInt(originalLines.size())
        : originalLines.size();
    const int simplifiedLineCount = hasSummary
        ? simplificationResult.value("simplifiedLineCount").toInt(simplifiedLines.size())
        : simplifiedLines.size();
    int linesRemoved = hasSummary
        ? simplificationResult.value("linesRemoved").toInt(originalLineCount - simplifiedLineCount)
        : (originalLineCount - simplifiedLineCount);
    linesRemoved = qMax(0, linesRemoved);
    
    double compressionRatio = hasSummary
        ? simplificationResult.value("compressionRatio").toDouble(
              originalLineCount > 0
                  ? 1.0 - static_cast<double>(simplifiedLineCount) / originalLineCount
                  : 0.0)
        : (originalLineCount > 0
              ? 1.0 - static_cast<double>(simplifiedLineCount) / originalLineCount
              : 0.0);
    compressionRatio = qBound(0.0, compressionRatio, 1.0);
    
    // Параметры
    int originalParams = hasSummary
        ? simplificationResult.value("originalParams").toInt(originalModelData.value("total_params").toInt())
        : originalModelData.value("total_params").toInt();
    int simplifiedParams = hasSummary
        ? simplificationResult.value("simplifiedParams").toInt(simplifiedModelData.value("total_params").toInt())
        : simplifiedModelData.value("total_params").toInt();
    double paramsCompression = (originalParams > 0)
        ? 1.0 - static_cast<double>(simplifiedParams) / originalParams
        : 0.0;
    paramsCompression = qBound(0.0, paramsCompression, 1.0);
    
    // Размер модели
    double originalSize = hasSummary
        ? simplificationResult.value("originalSizeMb").toDouble(originalModelData.value("model_size_mb").toDouble())
        : originalModelData.value("model_size_mb").toDouble();
    double simplifiedSize = hasSummary
        ? simplificationResult.value("simplifiedSizeMb").toDouble(simplifiedModelData.value("model_size_mb").toDouble())
        : simplifiedModelData.value("model_size_mb").toDouble();
    double sizeCompression = hasSummary
        ? simplificationResult.value("sizeReduction").toDouble(
              originalSize > 0.0 ? 1.0 - simplifiedSize / originalSize : 0.0)
        : (originalSize > 0.0 ? 1.0 - simplifiedSize / originalSize : 0.0);
    sizeCompression = qBound(0.0, sizeCompression, 1.0);
    
    // Точность - приоритет: сначала summary, потом из самих моделей
    QJsonValue originalAccuracyValue;
    QJsonValue simplifiedAccuracyValue;
    
    if (hasSummary) {
        originalAccuracyValue = simplificationResult.value("originalAccuracy");
        simplifiedAccuracyValue = simplificationResult.value("simplifiedAccuracy");
    }
    
    // Если в summary нет, берем из самих моделей
    if (!originalAccuracyValue.isDouble() && originalModelData.contains("accuracy")) {
        originalAccuracyValue = originalModelData.value("accuracy");
    }
    
    if (!simplifiedAccuracyValue.isDouble() && simplifiedModelData.contains("accuracy")) {
        simplifiedAccuracyValue = simplifiedModelData.value("accuracy");
    }
    
    bool hasOriginalAccuracy = originalAccuracyValue.isDouble();
    bool hasSimplifiedAccuracy = simplifiedAccuracyValue.isDouble();
    double originalAccuracy = hasOriginalAccuracy ? originalAccuracyValue.toDouble() : 0.0;
    double simplifiedAccuracy = hasSimplifiedAccuracy ? simplifiedAccuracyValue.toDouble() : 0.0;
    qDebug() << "  - summary available:" << hasSummary;
    qDebug() << "  - originalLineCount:" << originalLineCount << ", simplifiedLineCount:" << simplifiedLineCount;
    qDebug() << "  - linesRemoved:" << linesRemoved << ", compressionRatio:" << compressionRatio;
    qDebug() << "  - paramsCompression:" << paramsCompression << ", sizeCompression:" << sizeCompression;
    qDebug() << "  - accuracy (orig/simpl):" << (hasOriginalAccuracy ? originalAccuracy : -1)
             << (hasSimplifiedAccuracy ? simplifiedAccuracy : -1);
    
    // Обновляем метки с метриками
    if (comparisonLabel) {
        QString metrics;
        
        // Проверяем, есть ли данные о параметрах и размере
        if (originalParams > 0 && originalSize > 0.0) {
            // Полная версия с параметрами и размером
            metrics = QString::fromUtf8(
                "\xF0\x9F\x93\x8A Метрики упрощения:\n"
                "\xE2\x80\xA2 Удалено линий: %1 (%2%)\n"
                "\xE2\x80\xA2 Параметры: %3 \xE2\x86\x92 %4 (%5%)\n"
                "\xE2\x80\xA2 Размер модели: %6 MB \xE2\x86\x92 %7 MB (%8%)")
                .arg(linesRemoved)
                .arg(QString::number(compressionRatio * 100.0, 'f', 1))
                .arg(originalParams)
                .arg(simplifiedParams)
                .arg(QString::number(paramsCompression * 100.0, 'f', 1))
                .arg(QString::number(originalSize, 'f', 2))
                .arg(QString::number(simplifiedSize, 'f', 2))
                .arg(QString::number(sizeCompression * 100.0, 'f', 1));
        } else {
            // Упрощенная версия только с линиями
            metrics = QString::fromUtf8(
                "\xF0\x9F\x93\x8A Метрики упрощения:\n"
                "\xE2\x80\xA2 Удалено линий: %1 (%2%)\n"
                "\xE2\x80\xA2 Осталось линий: %3")
                .arg(linesRemoved)
                .arg(QString::number(compressionRatio * 100.0, 'f', 1))
                .arg(simplifiedLineCount);
        }
        
        if (hasOriginalAccuracy && hasSimplifiedAccuracy) {
            const double originalPercent = originalAccuracy * 100.0;
            const double simplifiedPercent = simplifiedAccuracy * 100.0;
            const double delta = simplifiedPercent - originalPercent;
            QString deltaStr = QString::number(delta, 'f', 2);
            if (delta > 0.0) {
                deltaStr.prepend('+');
            }
            metrics.append(QStringLiteral(
                "\n\xE2\x80\xA2 Точность: %1% → %2% (разница %3 п.п.)")
                .arg(QString::number(originalPercent, 'f', 2))
                .arg(QString::number(simplifiedPercent, 'f', 2))
                .arg(deltaStr));
        }
        
        comparisonLabel->setText(metrics);
        comparisonLabel->setStyleSheet("font-size: 14px; color: #495057; padding: 12px; background-color: #f8f9fa; border-radius: 8px;");
        comparisonLabel->setMinimumHeight(80);
        qDebug() << "  - Метрики обновлены:" << metrics;
    } else {
        qDebug() << "  - comparisonLabel равен nullptr";
    }
    
}

void ComparisonScene::onBackClicked()
{
    emit backRequested();
}

void ComparisonScene::onSaveClicked()
{
    QString filePath = newModelPathEdit->text();
    if (filePath.isEmpty()) {
        QMessageBox::warning(nullptr, "Предупреждение", "Пожалуйста, выберите путь для сохранения модели.");
        return;
    }
    
        emit saveRequested(filePath);
}

void ComparisonScene::onBrowseClicked()
{
    QString filePath = QFileDialog::getSaveFileName(nullptr, "Сохранить упрощенную модель", "", "JSON Files (*.json)");
    if (!filePath.isEmpty()) {
        newModelPathEdit->setText(filePath);
    }
}

void ComparisonScene::onTabChanged(int index)
{
    // Обновляем визуализацию при смене вкладки на вкладку с полигонами
    if (comparisonTabs && index >= 0) {
        QString tabText = comparisonTabs->tabText(index);
        if (tabText.contains("Полигоны") || tabText.contains("📊")) {
            qDebug() << "ComparisonScene::onTabChanged - переключение на вкладку полигонов";
            updatePolygonComparison();
        }
    }
}


void ComparisonScene::createMetricsChart()
{
    // Создаем круговую диаграмму для метрик
    QPieSeries *series = new QPieSeries();
    series->append("Сжатие", 0.0);
    series->append("Потеря точности", 0.0);
    series->append("Остальное", 100.0);
    
    // Настройка цветов
    series->slices().at(0)->setColor(QColor("#28a745")); // Зеленый для сжатия
    series->slices().at(1)->setColor(QColor("#dc3545")); // Красный для потери
    series->slices().at(2)->setColor(QColor("#6c757d")); // Серый для остального
    
    // Создаем диаграмму
    QChart *chart = new QChart();
    chart->addSeries(series);
    chart->setTitle("📊 Метрики производительности");
    chart->setTitleFont(QFont("Arial", 16, QFont::Bold));
    chart->legend()->setVisible(true);
    chart->legend()->setAlignment(Qt::AlignBottom);
    chart->legend()->setFont(QFont("Arial", 12));
    
    // Настройка виджета
    metricsChartView->setChart(chart);
    metricsChartView->setRenderHint(QPainter::Antialiasing);
    metricsChartView->setMinimumHeight(300);
    metricsChartView->setMaximumHeight(400);
    metricsChartView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void ComparisonScene::createParametersChart()
{
    // Создаем столбчатую диаграмму для параметров
    QBarSet *originalSet = new QBarSet("Оригинал");
    QBarSet *simplifiedSet = new QBarSet("Упрощенная");
    
    *originalSet << 7201;
    *simplifiedSet << 4275;
    
    QBarSeries *series = new QBarSeries();
    series->append(originalSet);
    series->append(simplifiedSet);
    
    // Настройка цветов
    originalSet->setColor(QColor("#007bff"));
    simplifiedSet->setColor(QColor("#28a745"));
    
    // Создаем диаграмму
    QChart *chart = new QChart();
    chart->addSeries(series);
    chart->setTitle("🔢 Количество параметров");
    chart->setTitleFont(QFont("Arial", 16, QFont::Bold));
    
    // Настройка осей
    QBarCategoryAxis *axisX = new QBarCategoryAxis();
    axisX->append("Модели");
    axisX->setLabelsFont(QFont("Arial", 12));
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);
    
    QValueAxis *axisY = new QValueAxis();
    axisY->setRange(0, 8000);
    axisY->setTitleText("Количество параметров");
    axisY->setTitleFont(QFont("Arial", 12));
    axisY->setLabelsFont(QFont("Arial", 10));
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);
    
    // Настройка виджета
    parametersChartView->setChart(chart);
    parametersChartView->setRenderHint(QPainter::Antialiasing);
    parametersChartView->setMinimumHeight(300);
    parametersChartView->setMaximumHeight(400);
    parametersChartView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void ComparisonScene::createSizeChart()
{
    // Создаем столбчатую диаграмму для размера модели
    QBarSet *originalSet = new QBarSet("Оригинал");
    QBarSet *simplifiedSet = new QBarSet("Упрощенная");
    
    *originalSet << 0.12;  // MB
    *simplifiedSet << 0.07; // MB
    
    QBarSeries *series = new QBarSeries();
    series->append(originalSet);
    series->append(simplifiedSet);
    
    // Настройка цветов
    originalSet->setColor(QColor("#ffc107"));
    simplifiedSet->setColor(QColor("#17a2b8"));
    
    // Создаем диаграмму
    QChart *chart = new QChart();
    chart->addSeries(series);
    chart->setTitle("💾 Размер модели");
    chart->setTitleFont(QFont("Arial", 16, QFont::Bold));
    
    // Настройка осей
    QBarCategoryAxis *axisX = new QBarCategoryAxis();
    axisX->append("Модели");
    axisX->setLabelsFont(QFont("Arial", 12));
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);
    
    QValueAxis *axisY = new QValueAxis();
    axisY->setRange(0, 0.15);
    axisY->setTitleText("Размер (MB)");
    axisY->setTitleFont(QFont("Arial", 12));
    axisY->setLabelsFont(QFont("Arial", 10));
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);
    
    // Настройка виджета
    sizeChartView->setChart(chart);
    sizeChartView->setRenderHint(QPainter::Antialiasing);
    sizeChartView->setMinimumHeight(300);
    sizeChartView->setMaximumHeight(400);
    sizeChartView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

