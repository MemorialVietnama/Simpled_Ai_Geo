#include "analysisscene.h"
#include <QFileInfo>
#include <QMessageBox>
#include <QJsonParseError>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QCoreApplication>
#include <QAbstractItemView>
#include <QTableWidgetItem>
#include <QColor>
#include <QRegularExpression>

AnalysisScene::AnalysisScene(QWidget *parent)
    : QWidget(parent)
    , analyzeButton(nullptr)
    , simplifyButton(nullptr)
    , backButton(nullptr)
    , modelTitleLabel(nullptr)
    , modelPathLabel(nullptr)
    , modelTree(nullptr)
    , logOutput(nullptr)
    , progressBar(nullptr)
    , mainTabWidget(nullptr)
    , overviewTable(nullptr)
    , layersTable(nullptr)
    , optimizerTable(nullptr)
    , metricsTable(nullptr)
    , weightsTextEdit(nullptr)
    , weightsTable(nullptr)
    , weightsTree(nullptr)
    , datasetTab(nullptr)
    , datasetInfoTable(nullptr)
    , datasetDataTable(nullptr)
    , datasetPreview(nullptr)
    , pythonProcess(nullptr)
{
    setupUI();
    
    // Initialize Python process with smart pointer
    pythonProcess = std::make_unique<QProcess>(this);
    connect(pythonProcess.get(), &QProcess::readyReadStandardOutput, this, &AnalysisScene::handlePythonOutput);
    connect(pythonProcess.get(), &QProcess::readyReadStandardError, this, &AnalysisScene::handlePythonError);
    connect(pythonProcess.get(), &QProcess::finished, this, &AnalysisScene::onPythonFinished);
}

AnalysisScene::~AnalysisScene()
{
    if (pythonProcess && pythonProcess->state() == QProcess::Running) {
        pythonProcess->terminate();
        if (!pythonProcess->waitForFinished(3000)) {
            pythonProcess->kill();
        }
    }
    // Smart pointer will automatically clean up the QProcess
}

void AnalysisScene::setupUI()
{
    // Minimalist background
    this->setStyleSheet("QWidget { background-color: #ffffff; }");
    
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(16);
    mainLayout->setContentsMargins(24, 24, 24, 24);

    // Clean header
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
    
    modelTitleLabel = std::make_unique<QLabel>("Анализ модели");
    modelTitleLabel->setStyleSheet(R"(
        font-size: 24px; 
        font-weight: 600; 
        color: #333333;
        padding: 8px 0;
    )");
    headerLayout->addWidget(modelTitleLabel.get());
    
    headerLayout->addStretch();
    
    mainLayout->addLayout(headerLayout);

    // Simple model path
    modelPathLabel = std::make_unique<QLabel>();
    modelPathLabel->setStyleSheet(R"(
        font-size: 13px; 
        color: #666666; 
        margin-bottom: 16px; 
        padding: 12px 16px; 
        background-color: #f9f9f9;
        border: 1px solid #e0e0e0;
        border-radius: 6px;
        font-weight: 400;
    )");
    modelPathLabel->setWordWrap(true);
    modelPathLabel->setMinimumHeight(40);
    mainLayout->addWidget(modelPathLabel.get());

    // Clean progress bar
    progressBar = std::make_unique<QProgressBar>();
    progressBar->setVisible(false);
    progressBar->setRange(0, 0); // Indeterminate progress
    progressBar->setMinimumHeight(24);
    progressBar->setStyleSheet(R"(
        QProgressBar {
            border: 1px solid #e0e0e0;
            border-radius: 4px;
            background-color: #f5f5f5;
            text-align: center;
            font-weight: 500;
            color: #666666;
        }
        QProgressBar::chunk {
            background-color: #333333;
            border-radius: 3px;
        }
    )");
    mainLayout->addWidget(progressBar.get());

    // Minimalist tab widget
    mainTabWidget = std::make_unique<QTabWidget>();
    mainTabWidget->setStyleSheet(R"(
        QTabWidget::pane {
            border: 1px solid #e0e0e0;
            border-radius: 6px;
            background-color: #ffffff;
            margin-top: -1px;
        }
        QTabBar::tab {
            background-color: #f5f5f5;
            border: 1px solid #e0e0e0;
            border-bottom: none;
            padding: 8px 16px;
            margin-right: 2px;
            color: #666666;
            border-top-left-radius: 6px;
            border-top-right-radius: 6px;
            font-weight: 500;
            font-size: 14px;
            min-width: 100px;
        }
        QTabBar::tab:selected {
            background-color: #ffffff;
            color: #333333;
            border-color: #e0e0e0;
            border-bottom: 1px solid #ffffff;
        }
        QTabBar::tab:hover:!selected {
            background-color: #f0f0f0;
            color: #333333;
        }
    )");

    // Clean table styling first
    QString tableStyle = R"(
        QTableWidget {
            background-color: #ffffff;
            border: none;
            border-radius: 6px;
            gridline-color: #f0f0f0;
            font-size: 13px;
        }
        QTableWidget::item {
            padding: 8px 12px;
            border-bottom: 1px solid #f0f0f0;
            color: #333333;
        }
        QTableWidget::item:selected {
            background-color: #f5f5f5;
            color: #333333;
        }
        QTableWidget::item:hover {
            background-color: #f8f8f8;
        }
        QHeaderView::section {
            background-color: #f5f5f5;
            color: #333333;
            padding: 8px 12px;
            border: none;
            font-weight: 600;
            font-size: 13px;
        }
    )";

    // Overview tab as table
    overviewTable = std::make_unique<QTableWidget>();
    overviewTable->setAlternatingRowColors(true);
    overviewTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    overviewTable->setStyleSheet(tableStyle);
    mainTabWidget->addTab(overviewTable.get(), "Обзор");

    // Layers table
    layersTable = std::make_unique<QTableWidget>();
    layersTable->setAlternatingRowColors(true);
    layersTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    layersTable->setSortingEnabled(true);
    layersTable->setStyleSheet(tableStyle);
    mainTabWidget->addTab(layersTable.get(), "Слои");

    // Optimizer tab
    optimizerTable = std::make_unique<QTableWidget>();
    optimizerTable->setAlternatingRowColors(true);
    optimizerTable->setStyleSheet(tableStyle);
    mainTabWidget->addTab(optimizerTable.get(), "Оптимизатор");

    // Metrics tab
    metricsTable = std::make_unique<QTableWidget>();
    metricsTable->setAlternatingRowColors(true);
    metricsTable->setStyleSheet(tableStyle);
    mainTabWidget->addTab(metricsTable.get(), "Метрики");

    // Dataset tab (training data) - расширенная версия с прокруткой
    datasetTab = std::make_unique<QWidget>();
    {
        // Создаем основной layout для вкладки
        QVBoxLayout *mainDsLayout = new QVBoxLayout(datasetTab.get());
        mainDsLayout->setContentsMargins(0, 0, 0, 0);
        mainDsLayout->setSpacing(0);
        
        // Создаем ScrollArea для прокрутки содержимого
        QScrollArea *scrollArea = new QScrollArea(datasetTab.get());
        scrollArea->setWidgetResizable(true);
        scrollArea->setFrameShape(QFrame::NoFrame);
        scrollArea->setStyleSheet(R"(
            QScrollArea {
                background-color: #ffffff;
                border: none;
            }
            QScrollBar:vertical {
                background-color: #f5f5f5;
                width: 12px;
                border: none;
            }
            QScrollBar::handle:vertical {
                background-color: #cccccc;
                min-height: 20px;
                border-radius: 6px;
            }
            QScrollBar::handle:vertical:hover {
                background-color: #aaaaaa;
            }
            QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
                height: 0px;
            }
        )");
        
        // Создаем виджет-контейнер для содержимого с прокруткой
        QWidget *scrollContent = new QWidget();
        QVBoxLayout *dsLayout = new QVBoxLayout(scrollContent);
        dsLayout->setContentsMargins(20, 20, 20, 20);
        dsLayout->setSpacing(20);  // Увеличенные отступы между секциями
        
        // Заголовок секции информации о файле
        QLabel *infoLabel = new QLabel("📊 Информация о датасете");
        infoLabel->setStyleSheet("font-size: 16px; font-weight: 600; color: #333333; padding-bottom: 8px;");
        dsLayout->addWidget(infoLabel);
        
        // Таблица с информацией о датасете
        datasetInfoTable = std::make_unique<QTableWidget>();
        datasetInfoTable->setColumnCount(2);
        datasetInfoTable->setHorizontalHeaderLabels(QStringList() << "Параметр" << "Значение");
        datasetInfoTable->setAlternatingRowColors(true);
        datasetInfoTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        datasetInfoTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        datasetInfoTable->setStyleSheet(tableStyle);
        datasetInfoTable->setFixedHeight(180);  // Фиксированная высота вместо минимальной
        datasetInfoTable->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        dsLayout->addWidget(datasetInfoTable.get());
        
        // Отступ между секциями
        QWidget *spacer1 = new QWidget();
        spacer1->setFixedHeight(10);
        spacer1->setStyleSheet("background: transparent;");
        dsLayout->addWidget(spacer1);
        
        // Заголовок секции обучающих данных
        QLabel *dataLabel = new QLabel("📋 Обучающие данные");
        dataLabel->setStyleSheet("font-size: 16px; font-weight: 600; color: #333333; padding-top: 8px; padding-bottom: 8px;");
        dsLayout->addWidget(dataLabel);
        
        // Таблица с обучающими данными
        datasetDataTable = std::make_unique<QTableWidget>();
        datasetDataTable->setAlternatingRowColors(true);
        datasetDataTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        datasetDataTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        datasetDataTable->setSortingEnabled(true);
        datasetDataTable->setStyleSheet(tableStyle);
        datasetDataTable->setFixedHeight(350);  // Фиксированная высота
        datasetDataTable->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        datasetDataTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        datasetDataTable->setAutoFillBackground(false);
        dsLayout->addWidget(datasetDataTable.get());
        
        // Отступ между секциями
        QWidget *spacer2 = new QWidget();
        spacer2->setFixedHeight(10);
        spacer2->setStyleSheet("background: transparent;");
        dsLayout->addWidget(spacer2);
        
        // Заголовок секции предпросмотра данных
        QLabel *previewLabel = new QLabel("👁️ Предпросмотр данных");
        previewLabel->setStyleSheet("font-size: 16px; font-weight: 600; color: #333333; padding-top: 8px; padding-bottom: 8px;");
        dsLayout->addWidget(previewLabel);
        
        // Предпросмотр данных
        datasetPreview = std::make_unique<QTextEdit>();
        datasetPreview->setReadOnly(true);
        datasetPreview->setFixedHeight(200);  // Фиксированная высота
        datasetPreview->setStyleSheet(R"(
            QTextEdit { 
                background:#f8f8f8; 
                border:1px solid #e0e0e0; 
                border-radius:6px; 
                padding:12px; 
                font-family:'Consolas','Monaco','Courier New',monospace; 
                font-size:11px;
                color: #333333;
            }
        )");
        dsLayout->addWidget(datasetPreview.get());
        
        // Добавляем растягивающийся элемент в конец
        dsLayout->addStretch();
        
        // Устанавливаем содержимое в ScrollArea
        scrollArea->setWidget(scrollContent);
        
        // Добавляем ScrollArea в основной layout
        mainDsLayout->addWidget(scrollArea);
    }
    mainTabWidget->addTab(datasetTab.get(), "📚 Обуч. данные");

    // Weights tab with multiple views
    QWidget *weightsWidget = new QWidget();
    QVBoxLayout *weightsLayout = new QVBoxLayout(weightsWidget);
    weightsLayout->setContentsMargins(0, 0, 0, 0);
    
    // Tab widget for different weight views
    QTabWidget *weightsTabWidget = new QTabWidget();
    weightsTabWidget->setStyleSheet(R"(
        QTabWidget::pane {
            border: 1px solid #e0e0e0;
            border-radius: 6px;
            background-color: #ffffff;
        }
        QTabBar::tab {
            background-color: #f5f5f5;
            color: #333333;
            padding: 8px 16px;
            margin-right: 2px;
            border-top-left-radius: 6px;
            border-top-right-radius: 6px;
        }
        QTabBar::tab:selected {
            background-color: #ffffff;
            border-bottom: 1px solid #ffffff;
        }
        QTabBar::tab:hover {
            background-color: #f8f8f8;
        }
    )");
    
    // Text view for detailed weights info
    weightsTextEdit = std::make_unique<QTextEdit>();
    weightsTextEdit->setReadOnly(true);
    weightsTextEdit->setFont(QFont("Consolas", 10));
    weightsTextEdit->setStyleSheet(R"(
        QTextEdit {
            background-color: #f8f8f8;
            color: #333333;
            border: none;
            padding: 12px;
            font-family: 'Consolas', 'Monaco', 'Courier New', monospace;
            font-size: 11px;
            line-height: 1.4;
        }
    )");
    weightsTabWidget->addTab(weightsTextEdit.get(), "Текст");
    
    // Table view for structured weights info
    weightsTable = std::make_unique<QTableWidget>();
    weightsTable->setAlternatingRowColors(true);
    weightsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    weightsTable->setSortingEnabled(true);
    weightsTable->setStyleSheet(tableStyle);
    weightsTabWidget->addTab(weightsTable.get(), "Таблица");
    
    // Tree view for hierarchical weights info
    weightsTree = std::make_unique<QTreeWidget>();
    weightsTree->setHeaderLabels(QStringList() << "Слой" << "Тип весов" << "Форма" << "Диапазон" << "Параметры");
    weightsTree->setAlternatingRowColors(true);
    weightsTree->setStyleSheet(R"(
        QTreeWidget {
            background-color: #ffffff;
            border: none;
            font-size: 13px;
            outline: none;
        }
        QTreeWidget::item {
            padding: 6px 12px;
            border-bottom: 1px solid #f0f0f0;
            color: #333333;
        }
        QTreeWidget::item:hover {
            background-color: #f8f8f8;
        }
        QTreeWidget::item:selected {
            background-color: #f5f5f5;
            color: #333333;
        }
        QHeaderView::section {
            background-color: #f5f5f5;
            color: #333333;
            padding: 8px 12px;
            border: none;
            font-weight: 600;
            font-size: 12px;
        }
    )");
    weightsTabWidget->addTab(weightsTree.get(), "Дерево");
    
    weightsLayout->addWidget(weightsTabWidget);
    mainTabWidget->addTab(weightsWidget, "Веса");

    // Tree view tab
    modelTree = std::make_unique<QTreeWidget>();
    modelTree->setHeaderLabels(QStringList() << "Слой/Информация" << "Тип/Значение" << "Параметры" << "Нейроны" << "Форма входа" << "Форма выхода/Размер");
    modelTree->setAlternatingRowColors(true);
    modelTree->setStyleSheet(R"(
        QTreeWidget {
            background-color: #ffffff;
            border: 1px solid #e0e0e0;
            border-radius: 6px;
            font-size: 13px;
            outline: none;
        }
        QTreeWidget::item {
            padding: 6px 12px;
            border-bottom: 1px solid #f0f0f0;
            color: #333333;
        }
        QTreeWidget::item:hover {
            background-color: #f8f8f8;
        }
        QTreeWidget::item:selected {
            background-color: #f5f5f5;
            color: #333333;
        }
        QHeaderView::section {
            background-color: #f5f5f5;
            color: #333333;
            padding: 8px 12px;
            border: none;
            font-weight: 600;
            font-size: 12px;
        }
    )");
    mainTabWidget->addTab(modelTree.get(), "Дерево");

    mainLayout->addWidget(mainTabWidget.get());

    // Minimalist buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(12);
    
    analyzeButton = std::make_unique<QPushButton>("Анализировать модель");
    analyzeButton->setStyleSheet(R"(
        QPushButton {
            background-color: #333333;
            color: white;
            border: none;
            border-radius: 6px;
            padding: 12px 24px;
            font-weight: 500;
            font-size: 14px;
            min-height: 40px;
        }
        QPushButton:hover {
            background-color: #555555;
        }
        QPushButton:pressed {
            background-color: #222222;
        }
        QPushButton:disabled {
            background-color: #cccccc;
            color: #999999;
        }
    )");
    buttonLayout->addWidget(analyzeButton.get());
    
    buttonLayout->addStretch();
    
    simplifyButton = std::make_unique<QPushButton>("Упростить модель");
    simplifyButton->setObjectName("simplifyButton");
    simplifyButton->setStyleSheet(R"(
        QPushButton {
            background-color: #666666;
            color: white;
            border: none;
            border-radius: 6px;
            padding: 12px 24px;
            font-weight: 500;
            font-size: 14px;
            min-height: 40px;
        }
        QPushButton:hover {
            background-color: #777777;
        }
        QPushButton:pressed {
            background-color: #555555;
        }
        QPushButton:disabled {
            background-color: #cccccc;
            color: #999999;
        }
    )");
    simplifyButton->setEnabled(false); // Initially disabled
    buttonLayout->addWidget(simplifyButton.get());
    
    mainLayout->addLayout(buttonLayout);

    // Clean log output
    QLabel *logLabel = new QLabel("Лог выполнения:");
    logLabel->setStyleSheet(R"(
        font-weight: 600; 
        font-size: 14px; 
        color: #333333;
        margin-top: 16px;
        margin-bottom: 8px;
    )");
    mainLayout->addWidget(logLabel);
    
    logOutput = std::make_unique<QTextEdit>();
    logOutput->setMaximumHeight(120);
    logOutput->setMinimumHeight(80);
    logOutput->setReadOnly(true);
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
        QTextEdit:focus {
            border-color: #cccccc;
        }
    )");
    mainLayout->addWidget(logOutput.get());

    // Connect signals
    connect(backButton.get(), &QPushButton::clicked, this, &AnalysisScene::backRequested);
    connect(analyzeButton.get(), &QPushButton::clicked, this, &AnalysisScene::startAnalysis);
    connect(simplifyButton.get(), &QPushButton::clicked, this, &AnalysisScene::simplifyRequested);
}

void AnalysisScene::setTrainingDataPath(const QString &filePath)
{
    qDebug() << "[AnalysisScene] setTrainingDataPath: начало, путь =" << filePath;
    appendLogMessage("📂 Выбран файл обучающих данных: " + filePath);

    trainingDataPath = filePath;
    qDebug() << "[AnalysisScene] setTrainingDataPath: путь установлен, значение =" << trainingDataPath;

    if (trainingDataPath.isEmpty()) {
        qDebug() << "[AnalysisScene] setTrainingDataPath: ПРЕДУПРЕЖДЕНИЕ - путь пуст";
        appendLogMessage("⚠️ Путь к датасету пуст — данные не будут отображены.");
        return;
    }

    QFileInfo fi(trainingDataPath);
    qDebug() << "[AnalysisScene] setTrainingDataPath: проверка файла - существует =" << fi.exists() << ", читаемый =" << fi.isReadable() << ", размер =" << fi.size() << "байт";
    if (!fi.exists() || !fi.isReadable()) {
        qDebug() << "[AnalysisScene] setTrainingDataPath: ОШИБКА - файл не найден или нет доступа";
        appendLogMessage("❌ Файл не найден или нет доступа: " + trainingDataPath);
        return;
    }

    qDebug() << "[AnalysisScene] setTrainingDataPath: файл валиден, начинаем загрузку данных";
    appendLogMessage("🔍 Загружаем и анализируем обучающие данные...");
    populateTrainingDataInfo(trainingDataPath);
    qDebug() << "[AnalysisScene] setTrainingDataPath: загрузка завершена";
}


void AnalysisScene::setModelPath(const QString &filePath)
{
    qDebug() << "[AnalysisScene] setModelPath: установка пути к модели =" << filePath;
    currentFilePath = filePath;
    QFileInfo fileInfo(filePath);
    qDebug() << "[AnalysisScene] setModelPath: информация о файле - существует =" << fileInfo.exists() << ", размер =" << fileInfo.size() << "байт";
    modelTitleLabel->setText("Анализ модели: " + fileInfo.fileName());
    modelPathLabel->setText("📁 " + filePath);
    qDebug() << "[AnalysisScene] setModelPath: UI обновлен";
}

void AnalysisScene::populateTrainingDataInfo(const QString &filePath)
{
    appendLogMessage("📥 Чтение данных из файла: " + filePath);
    qDebug() << "[Dataset] Читаем:" << filePath;

    if (!datasetInfoTable || !datasetDataTable) {
        qDebug() << "[Dataset] Таблицы не инициализированы!";
        return;
    }

    QFileInfo fi(filePath);
    QString ext = fi.suffix().toLower();

    datasetInfoTable->clear();
    datasetInfoTable->setColumnCount(2);
    datasetInfoTable->setHorizontalHeaderLabels({"Параметр", "Значение"});
    datasetInfoTable->setRowCount(0);

    // Базовая инфа
    auto addInfo = [&](QString key, QString value){
        int r = datasetInfoTable->rowCount();
        datasetInfoTable->insertRow(r);
        datasetInfoTable->setItem(r, 0, createTableItem(key));
        datasetInfoTable->setItem(r, 1, createTableItem(value));
    };

    addInfo("Имя файла", fi.fileName());
    addInfo("Путь", filePath);
    addInfo("Размер", QString::number(fi.size()/1024.0, 'f', 2) + " KB");
    addInfo("Тип", ext.toUpper());

    datasetDataTable->clear();
    datasetDataTable->setRowCount(0);

    // ---- CSV ----
    if (ext == "csv") {
        appendLogMessage("🔎 CSV: анализируем структуру...");

        QFile f(filePath);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            appendLogMessage("❌ Ошибка чтения CSV");
            return;
        }

        QTextStream in(&f);
        QString headerLine = in.readLine();
        QStringList headers = parseCSVLine(headerLine);

        datasetDataTable->setColumnCount(headers.size());
        datasetDataTable->setHorizontalHeaderLabels(headers);

        int rowCount = 0;
        while (!in.atEnd() && rowCount < 500) {
            QStringList values = parseCSVLine(in.readLine());
            datasetDataTable->insertRow(rowCount);

            for (int c = 0; c < headers.size(); c++)
                datasetDataTable->setItem(rowCount, c, createTableItem(values.value(c, "")));

            rowCount++;
        }
        addInfo("Строк загружено", QString::number(rowCount));
        appendLogMessage("✅ CSV загружен успешно (" + QString::number(rowCount) + " строк)");

        return;
    }

    // ---- TXT ----
    if (ext == "txt") {
        appendLogMessage("🔎 TXT: отображаем как текстовый список");

        datasetDataTable->setColumnCount(1);
        datasetDataTable->setHorizontalHeaderLabels({"Строка"});

        QFile f(filePath);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;

        QTextStream in(&f);
        int row = 0;
        while (!in.atEnd() && row < 1000) {
            datasetDataTable->insertRow(row);
            datasetDataTable->setItem(row, 0, createTableItem(in.readLine()));
            row++;
        }
        addInfo("Строк", QString::number(row));

        appendLogMessage("✅ TXT загружен успешно");
        return;
    }

    // ---- JSON ----
    if (ext == "json") {
        appendLogMessage("🔎 JSON: загружаем объект...");

        QFile f(filePath);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;

        QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        if (!doc.isArray()) {
            appendLogMessage("⚠️ JSON не является массивом → отображаем как текст");
            datasetPreview->setPlainText(QString(doc.toJson(QJsonDocument::Indented)));
            return;
        }

        QJsonArray arr = doc.array();
        if (arr.isEmpty()) return;

        QStringList keys = arr[0].toObject().keys();
        datasetDataTable->setColumnCount(keys.size());
        datasetDataTable->setHorizontalHeaderLabels(keys);

        for (int i = 0; i < arr.size() && i < 500; i++) {
            datasetDataTable->insertRow(i);
            QJsonObject obj = arr[i].toObject();
            for (int k = 0; k < keys.size(); k++)
                datasetDataTable->setItem(i, k, createTableItem(obj[keys[k]].toVariant().toString()));
        }

        addInfo("Объектов", QString::number(arr.size()));
        appendLogMessage("✅ JSON загружен");

        return;
    }

    // ---- Другие форматы ----
    appendLogMessage("⚠️ Формат не поддерживается: " + ext);
}

void AnalysisScene::populateTrainingDataFromJson(const QJsonObject &trainingData)
{
    appendLogMessage("📊 Заполняем данные об обучающих данных из JSON анализа...");
    qDebug() << "[Dataset] Заполняем из JSON:" << trainingData.keys();

    if (!datasetInfoTable || !datasetDataTable || !datasetPreview) {
        qDebug() << "[Dataset] Таблицы не инициализированы!";
        return;
    }

    // Очищаем таблицы
    datasetInfoTable->clear();
    datasetInfoTable->setColumnCount(2);
    datasetInfoTable->setHorizontalHeaderLabels({"Параметр", "Значение"});
    datasetInfoTable->setRowCount(0);

    datasetDataTable->clear();
    datasetDataTable->setRowCount(0);

    // Вспомогательная функция для добавления информации
    auto addInfo = [&](QString key, QString value) {
        int r = datasetInfoTable->rowCount();
        datasetInfoTable->insertRow(r);
        datasetInfoTable->setItem(r, 0, createTableItem(key));
        datasetInfoTable->setItem(r, 1, createTableItem(value));
    };

    // Базовая информация
    if (trainingData.contains("data_name")) {
        addInfo("Имя файла", trainingData["data_name"].toString());
    }
    if (trainingData.contains("data_path")) {
        addInfo("Путь", trainingData["data_path"].toString());
    }
    if (trainingData.contains("data_size_kb")) {
        double sizeKb = trainingData["data_size_kb"].toDouble();
        addInfo("Размер", QString::number(sizeKb, 'f', 2) + " KB");
        if (trainingData.contains("data_size_mb")) {
            double sizeMb = trainingData["data_size_mb"].toDouble();
            addInfo("Размер (MB)", QString::number(sizeMb, 'f', 2) + " MB");
        }
    }
    if (trainingData.contains("format")) {
        addInfo("Формат", trainingData["format"].toString());
    }
    if (trainingData.contains("file_extension")) {
        addInfo("Расширение", trainingData["file_extension"].toString());
    }

    // Обработка CSV данных
    if (trainingData.contains("format") && trainingData["format"].toString() == "CSV") {
        appendLogMessage("📋 Заполняем CSV данные из JSON...");
        
        if (trainingData.contains("column_names")) {
            QJsonArray columns = trainingData["column_names"].toArray();
            QStringList headers;
            for (const QJsonValue &col : columns) {
                headers << col.toString();
            }
            datasetDataTable->setColumnCount(headers.size());
            datasetDataTable->setHorizontalHeaderLabels(headers);
            
            if (trainingData.contains("rows")) {
                addInfo("Строк", QString::number(trainingData["rows"].toInt()));
            }
            if (trainingData.contains("columns")) {
                addInfo("Колонок", QString::number(trainingData["columns"].toInt()));
            }
        }

        // Заполняем предпросмотр данных
        if (trainingData.contains("preview")) {
            QJsonArray preview = trainingData["preview"].toArray();
            
            // Получаем порядок колонок из column_names
            QStringList columnOrder;
            if (trainingData.contains("column_names")) {
                QJsonArray columns = trainingData["column_names"].toArray();
                for (const QJsonValue &col : columns) {
                    columnOrder << col.toString();
                }
            }
            
            int rowCount = 0;
            for (const QJsonValue &rowVal : preview) {
                if (rowCount >= 100) break; // Ограничиваем количество строк
                QJsonObject rowObj = rowVal.toObject();
                datasetDataTable->insertRow(rowCount);
                
                // Используем порядок из column_names, если он есть
                QStringList keysToUse = columnOrder.isEmpty() ? rowObj.keys() : columnOrder;
                
                int col = 0;
                for (const QString &key : keysToUse) {
                    if (col < datasetDataTable->columnCount() && rowObj.contains(key)) {
                        QJsonValue val = rowObj[key];
                        QString valStr;
                        if (val.isString()) {
                            valStr = val.toString();
                        } else if (val.isDouble()) {
                            valStr = QString::number(val.toDouble(), 'g', 6);
                        } else if (val.isBool()) {
                            valStr = val.toBool() ? "true" : "false";
                        } else {
                            valStr = val.toVariant().toString();
                        }
                        datasetDataTable->setItem(rowCount, col, createTableItem(valStr));
                        col++;
                    } else if (col < datasetDataTable->columnCount()) {
                        // Если ключа нет в строке, оставляем ячейку пустой
                        datasetDataTable->setItem(rowCount, col, createTableItem(""));
                        col++;
                    }
                }
                rowCount++;
            }
            appendLogMessage("✅ Загружено " + QString::number(rowCount) + " строк предпросмотра");
        }

        // Статистика
        if (trainingData.contains("statistics")) {
            QJsonObject stats = trainingData["statistics"].toObject();
            QString statsText = "Статистика:\n";
            for (const QString &col : stats.keys()) {
                QJsonObject colStats = stats[col].toObject();
                statsText += QString("\n%1:\n").arg(col);
                if (colStats.contains("mean")) statsText += QString("  Среднее: %1\n").arg(colStats["mean"].toDouble(), 0, 'g', 6);
                if (colStats.contains("std")) statsText += QString("  Стд. откл.: %1\n").arg(colStats["std"].toDouble(), 0, 'g', 6);
                if (colStats.contains("min")) statsText += QString("  Мин: %1\n").arg(colStats["min"].toDouble(), 0, 'g', 6);
                if (colStats.contains("max")) statsText += QString("  Макс: %1\n").arg(colStats["max"].toDouble(), 0, 'g', 6);
            }
            datasetPreview->setPlainText(statsText);
        }
    }
    // Обработка JSON данных
    else if (trainingData.contains("format") && trainingData["format"].toString().contains("JSON")) {
        appendLogMessage("📋 Заполняем JSON данные из анализа...");
        
        bool isArray = trainingData.contains("is_array") && trainingData["is_array"].toBool();
        
        if (isArray && trainingData.contains("keys")) {
            QJsonArray keysArray = trainingData["keys"].toArray();
            QStringList keys;
            for (const QJsonValue &key : keysArray) {
                keys << key.toString();
            }
            datasetDataTable->setColumnCount(keys.size());
            datasetDataTable->setHorizontalHeaderLabels(keys);
            
            if (trainingData.contains("items_count")) {
                addInfo("Элементов", QString::number(trainingData["items_count"].toInt()));
            }
            
            if (trainingData.contains("preview")) {
                QJsonArray preview = trainingData["preview"].toArray();
                int row = 0;
                for (const QJsonValue &itemVal : preview) {
                    if (row >= 500) break;
                    QJsonObject itemObj = itemVal.toObject();
                    datasetDataTable->insertRow(row);
                    int col = 0;
                    for (const QString &key : keys) {
                        if (itemObj.contains(key) && col < datasetDataTable->columnCount()) {
                            QString valStr = itemObj[key].toVariant().toString();
                            datasetDataTable->setItem(row, col, createTableItem(valStr));
                        }
                        col++;
                    }
                    row++;
                }
                appendLogMessage("✅ Загружено " + QString::number(row) + " элементов");
            }
            
            // Предпросмотр как JSON
            QJsonDocument previewDoc;
            if (trainingData.contains("preview")) {
                previewDoc = QJsonDocument(trainingData["preview"].toArray());
            }
            datasetPreview->setPlainText(QString(previewDoc.toJson(QJsonDocument::Indented)));
        } else {
            // JSON объект или примитив
            QJsonDocument previewDoc;
            if (trainingData.contains("preview")) {
                if (trainingData["preview"].isObject()) {
                    previewDoc = QJsonDocument(trainingData["preview"].toObject());
                } else {
                    previewDoc = QJsonDocument(trainingData["preview"].toArray());
                }
            }
            datasetPreview->setPlainText(QString(previewDoc.toJson(QJsonDocument::Indented)));
        }
    }
    // Обработка NPZ/NPY данных
    else if (trainingData.contains("format") && 
             (trainingData["format"].toString() == "NPZ" || trainingData["format"].toString() == "NPY")) {
        appendLogMessage("📋 Заполняем NumPy данные из анализа...");
        
        if (trainingData["format"].toString() == "NPZ" && trainingData.contains("arrays_info")) {
            QJsonObject arraysInfo = trainingData["arrays_info"].toObject();
            
            datasetDataTable->setColumnCount(4);
            datasetDataTable->setHorizontalHeaderLabels({"Массив", "Форма", "Тип", "Размер"});
            
            int row = 0;
            for (const QString &key : arraysInfo.keys()) {
                QJsonObject arrInfo = arraysInfo[key].toObject();
                datasetDataTable->insertRow(row);
                datasetDataTable->setItem(row, 0, createTableItem(key));
                
                if (arrInfo.contains("shape")) {
                    QString shapeStr = arrInfo["shape"].toString();
                    datasetDataTable->setItem(row, 1, createTableItem(shapeStr));
                }
                if (arrInfo.contains("dtype")) {
                    datasetDataTable->setItem(row, 2, createTableItem(arrInfo["dtype"].toString()));
                }
                if (arrInfo.contains("size")) {
                    datasetDataTable->setItem(row, 3, createTableItem(QString::number(arrInfo["size"].toInt())));
                }
                row++;
            }
            addInfo("Массивов", QString::number(row));
        } else if (trainingData.contains("shape")) {
            QString shapeStr = trainingData["shape"].toString();
            addInfo("Форма", shapeStr);
            if (trainingData.contains("dtype")) {
                addInfo("Тип данных", trainingData["dtype"].toString());
            }
            if (trainingData.contains("size")) {
                addInfo("Размер", QString::number(trainingData["size"].toInt()));
            }
            if (trainingData.contains("min") && trainingData.contains("max")) {
                addInfo("Диапазон", QString("[%1, %2]")
                    .arg(trainingData["min"].toDouble(), 0, 'g', 6)
                    .arg(trainingData["max"].toDouble(), 0, 'g', 6));
            }
        }
    }
    // Обработка TXT данных
    else if (trainingData.contains("format") && trainingData["format"].toString() == "Text") {
        appendLogMessage("📋 Заполняем текстовые данные из анализа...");
        
        datasetDataTable->setColumnCount(1);
        datasetDataTable->setHorizontalHeaderLabels({"Строка"});
        
        if (trainingData.contains("preview_lines")) {
            QJsonArray lines = trainingData["preview_lines"].toArray();
            int row = 0;
            for (const QJsonValue &line : lines) {
                datasetDataTable->insertRow(row);
                datasetDataTable->setItem(row, 0, createTableItem(line.toString()));
                row++;
            }
            if (trainingData.contains("lines_count")) {
                addInfo("Всего строк", QString::number(trainingData["lines_count"].toInt()));
                addInfo("Загружено строк", QString::number(row));
            }
            
            // Предпросмотр
            QString previewText;
            for (const QJsonValue &line : lines) {
                previewText += line.toString() + "\n";
            }
            datasetPreview->setPlainText(previewText);
        }
    }
    // Ошибка или неподдерживаемый формат
    else if (trainingData.contains("error")) {
        QString errorMsg = trainingData["error"].toString();
        appendLogMessage("❌ Ошибка анализа данных: " + errorMsg);
        addInfo("Ошибка", errorMsg);
    }

    appendLogMessage("✅ Данные об обучающих данных загружены из JSON");
}

QStringList AnalysisScene::parseCSVLine(const QString &line)
{
    QStringList result;
    QString current;
    bool inQuotes = false;
    
    for (int i = 0; i < line.length(); i++) {
        QChar ch = line[i];
        
        if (ch == '"') {
            if (inQuotes && i + 1 < line.length() && line[i + 1] == '"') {
                // Двойные кавычки - экранированная кавычка
                current += '"';
                i++; // Пропускаем следующую кавычку
            } else {
                // Переключаем состояние внутри/вне кавычек
                inQuotes = !inQuotes;
            }
        } else if (ch == ',' && !inQuotes) {
            // Разделитель вне кавычек
            result.append(current.trimmed());
            current.clear();
        } else {
            current += ch;
        }
    }
    
    // Добавляем последнее поле
    result.append(current.trimmed());
    
    return result;
}

QTableWidgetItem* AnalysisScene::createTableItem(const QString &text)
{
    QTableWidgetItem *item = new QTableWidgetItem(text);
    item->setForeground(QColor("#333333"));
    item->setBackground(QColor("#ffffff"));
    return item;
}

void AnalysisScene::clearData()
{
    modelTree->clear();
    if (logOutput) {
        logOutput->clear();
    }
    emit analysisLogReset();
    simplifyButton->setEnabled(false);
}

void AnalysisScene::startAnalysis()
{
    qDebug() << "[AnalysisScene] startAnalysis: ===== НАЧАЛО АНАЛИЗА =====";
    qDebug() << "[AnalysisScene] startAnalysis: путь к модели =" << currentFilePath;
    qDebug() << "[AnalysisScene] startAnalysis: путь к обучающим данным =" << trainingDataPath;
    
    try {
        if (logOutput) {
            logOutput->clear();
        }
        emit analysisLogReset();
        
        if (currentFilePath.isEmpty()) {
            qDebug() << "[AnalysisScene] startAnalysis: ОШИБКА - путь к модели не установлен";
            QMessageBox::warning(this, "Ошибка", "Сначала выберите модель");
            return;
        }
        
        qDebug() << "[AnalysisScene] startAnalysis: валидация пути к модели";
        // Validate file path for security
        if (!isValidFilePath(currentFilePath)) {
            qDebug() << "[AnalysisScene] startAnalysis: ОШИБКА БЕЗОПАСНОСТИ - недопустимый путь к файлу модели";
            QMessageBox::critical(this, "Ошибка безопасности", "Недопустимый путь к файлу");
            return;
        }
        qDebug() << "[AnalysisScene] startAnalysis: путь к модели валиден";
    
    // Check Python environment first
    qDebug() << "[AnalysisScene] startAnalysis: проверка Python окружения";
    if (!checkPythonEnvironment()) {
        qDebug() << "[AnalysisScene] startAnalysis: ОШИБКА - Python окружение не найдено или зависимости не установлены";
        showDependencyError();
        return;
    }
    qDebug() << "[AnalysisScene] startAnalysis: Python окружение проверено успешно";
    
    // Enhanced loading state
    qDebug() << "[AnalysisScene] startAnalysis: настройка UI для анализа";
    appendLogMessage("🔍 Начинаем анализ модели...");
    appendLogMessage("📁 Путь к модели: " + currentFilePath);
    if (!trainingDataPath.isEmpty()) {
        appendLogMessage("📚 Путь к обучающим данным: " + trainingDataPath);
        qDebug() << "[AnalysisScene] startAnalysis: обучающие данные включены в анализ";
    }
    progressBar->setVisible(true);
    progressBar->setFormat("⏳ Анализируем модель... %p%");
    
    // Disable buttons during analysis
    analyzeButton->setEnabled(false);
    analyzeButton->setText("⏳ Анализируем...");
    simplifyButton->setEnabled(false);
    qDebug() << "[AnalysisScene] startAnalysis: кнопки отключены";
    
    // Clear previous data
    qDebug() << "[AnalysisScene] startAnalysis: очистка предыдущих данных";
    clearData();
    
    // Очищаем буфер вывода
    pythonOutputBuffer.clear();
    qDebug() << "[AnalysisScene] startAnalysis: буфер вывода очищен";
    
    // Check if virtual environment exists
    qDebug() << "[AnalysisScene] startAnalysis: поиск Python интерпретатора";
    QString venvPath = "venv";
    QString pythonExe;
    
    // Try to find venv in different locations
    QStringList possibleVenvPaths = {
        "venv",                                                    // Current directory
        "../venv",                                                 // Parent directory
        "../../venv",                                              // Two levels up
        QCoreApplication::applicationDirPath() + "/venv",          // Same as exe
        QCoreApplication::applicationDirPath() + "/../venv",       // Parent of exe
        QCoreApplication::applicationDirPath() + "/../../venv"     // Two levels up from exe
    };
    
    qDebug() << "[AnalysisScene] startAnalysis: проверка путей venv:";
    for (const QString &path : possibleVenvPaths) {
        qDebug() << "[AnalysisScene] startAnalysis:   -" << path;
    }
    
    bool venvFound = false;
    for (const QString &path : possibleVenvPaths) {
        if (QDir(path).exists()) {
            qDebug() << "[AnalysisScene] startAnalysis: найден каталог venv:" << path;
            QString venvPython = QDir(path).absoluteFilePath("Scripts/python.exe");
            qDebug() << "[AnalysisScene] startAnalysis: проверка Python exe:" << venvPython;
            if (QFile::exists(venvPython)) {
                pythonExe = venvPython;
                venvPath = path;
                venvFound = true;
                qDebug() << "[AnalysisScene] startAnalysis: ✓ venv найден и используется:" << pythonExe;
                appendLogMessage("🐍 Найдено виртуальное окружение: " + venvPath);
                appendLogMessage("🐍 Используем Python: " + pythonExe);
                break;
            } else {
                qDebug() << "[AnalysisScene] startAnalysis: Python exe не найден по пути:" << venvPython;
            }
        }
    }
    
    if (!venvFound) {
        // Use system Python
        pythonExe = "python";
        qDebug() << "[AnalysisScene] startAnalysis: venv не найден, используем системный Python:" << pythonExe;
        appendLogMessage("🐍 Виртуальное окружение не найдено, используем системный Python");
        appendLogMessage("💡 Для создания venv запустите: install_dependencies.bat");
    }
    
    // Start Python analysis script - find it relative to executable
    qDebug() << "[AnalysisScene] startAnalysis: поиск скрипта analyze_model.py";
    QString scriptPath;
    QString exeDir = QCoreApplication::applicationDirPath();
    QString currentDir = QDir::currentPath();
    qDebug() << "[AnalysisScene] startAnalysis: exeDir =" << exeDir;
    qDebug() << "[AnalysisScene] startAnalysis: currentDir =" << currentDir;
    
    // Try different possible locations for the script
    QStringList possiblePaths = {
        exeDir + "/analyze_model.py",                    // Same directory as exe
        exeDir + "/../analyze_model.py",                 // Parent directory
        exeDir + "/../../analyze_model.py",              // Two levels up
        currentDir + "/analyze_model.py",                // Current working directory
        currentDir + "/../analyze_model.py",             // Parent of current directory
        "analyze_model.py"                               // In PATH or current directory
    };
    
    qDebug() << "[AnalysisScene] startAnalysis: проверка путей скрипта:";
    for (const QString &path : possiblePaths) {
        bool exists = QFile::exists(path);
        qDebug() << "[AnalysisScene] startAnalysis:   -" << path << (exists ? "✓ НАЙДЕН" : "✗ не найден");
        if (exists && scriptPath.isEmpty()) {
            scriptPath = path;
            qDebug() << "[AnalysisScene] startAnalysis: ✓ скрипт найден:" << scriptPath;
        }
    }
    
    if (scriptPath.isEmpty()) {
        qDebug() << "[AnalysisScene] startAnalysis: ОШИБКА - скрипт analyze_model.py не найден";
        appendLogMessage("❌ Ошибка: Файл analyze_model.py не найден!");
        appendLogMessage("Искали в следующих местах:");
        for (const QString &path : possiblePaths) {
            appendLogMessage("  - " + path);
        }
        progressBar->setVisible(false);
        return;
    }
    
    // Sanitize arguments for security
    qDebug() << "[AnalysisScene] startAnalysis: подготовка аргументов для Python скрипта";
    QStringList args;
    args << QDir::toNativeSeparators(scriptPath) << QDir::toNativeSeparators(currentFilePath);
    qDebug() << "[AnalysisScene] startAnalysis: аргументы (базовые):" << args;
    
    // Добавляем путь к обучающим данным, если он указан
    if (!trainingDataPath.isEmpty()) {
        qDebug() << "[AnalysisScene] startAnalysis: проверка пути к обучающим данным:" << trainingDataPath;
        // Валидация пути к обучающим данным
        QFileInfo trainingFileInfo(trainingDataPath);
        if (!trainingFileInfo.exists() || !trainingFileInfo.isReadable()) {
            qDebug() << "[AnalysisScene] startAnalysis: ПРЕДУПРЕЖДЕНИЕ - файл обучающих данных недоступен";
            appendLogMessage("⚠️ Предупреждение: Файл обучающих данных не найден или недоступен: " + trainingDataPath);
            appendLogMessage("📊 Анализ модели будет выполнен без анализа обучающих данных");
        } else {
            args << QDir::toNativeSeparators(trainingDataPath);
            qDebug() << "[AnalysisScene] startAnalysis: ✓ обучающие данные добавлены в аргументы";
            appendLogMessage("📚 Добавлены обучающие данные: " + trainingDataPath);
        }
    } else {
        qDebug() << "[AnalysisScene] startAnalysis: обучающие данные не указаны";
        appendLogMessage("ℹ️ Обучающие данные не указаны — будет выполнен только анализ модели");
    }
    
    qDebug() << "[AnalysisScene] startAnalysis: финальные аргументы:" << args;
    
    // Validate Python executable path
    qDebug() << "[AnalysisScene] startAnalysis: валидация пути к Python:" << pythonExe;
    if (!isValidPythonPath(pythonExe)) {
        qDebug() << "[AnalysisScene] startAnalysis: ОШИБКА БЕЗОПАСНОСТИ - недопустимый путь к Python";
        appendLogMessage("❌ Ошибка безопасности: Недопустимый путь к Python");
        progressBar->setVisible(false);
        return;
    }
    qDebug() << "[AnalysisScene] startAnalysis: путь к Python валиден";
    
    QString commandLine = pythonExe + " " + scriptPath + " " + currentFilePath;
    if (!trainingDataPath.isEmpty()) {
        commandLine += " " + trainingDataPath;
    }
    qDebug() << "[AnalysisScene] startAnalysis: команда для запуска:" << commandLine;
    appendLogMessage("▶️ Запускаем: " + commandLine);
    
    // Set working directory for security
    QString workingDir = QCoreApplication::applicationDirPath();
    qDebug() << "[AnalysisScene] startAnalysis: рабочая директория:" << workingDir;
    pythonProcess->setWorkingDirectory(workingDir);
    
    qDebug() << "[AnalysisScene] startAnalysis: запуск Python процесса...";
    pythonProcess->start(pythonExe, args);
    qDebug() << "[AnalysisScene] startAnalysis: процесс запущен, состояние:" << pythonProcess->state();
    
        if (!pythonProcess->waitForStarted(5000)) {
            qDebug() << "[AnalysisScene] startAnalysis: ОШИБКА - процесс не запустился за 5 секунд";
            qDebug() << "[AnalysisScene] startAnalysis: код ошибки:" << pythonProcess->error();
            qDebug() << "[AnalysisScene] startAnalysis: строка ошибки:" << pythonProcess->errorString();
            appendLogMessage("❌ Ошибка запуска Python процесса");
            progressBar->setVisible(false);
            QMessageBox::critical(this, "Ошибка", "Не удалось запустить Python. Убедитесь, что Python установлен и доступен в PATH.");
        } else {
            qDebug() << "[AnalysisScene] startAnalysis: ✓ Python процесс успешно запущен";
        }
    } catch (const std::exception &e) {
        handleException(e, "startAnalysis");
    }
}

bool AnalysisScene::isValidFilePath(const QString &filePath)
{
    try {
        QFileInfo fileInfo(filePath);
        
        // Check for path traversal attacks
        if (filePath.contains("..") || filePath.contains("~")) {
            return false;
        }
        
        // Check if file exists and is readable
        if (!fileInfo.exists() || !fileInfo.isReadable()) {
            return false;
        }
        
        // Check file extension for security
        QString extension = fileInfo.suffix().toLower();
        QStringList allowedExtensions = {"h5", "hdf5", "pb", "pkl", "pth", "pt", "onnx", "tflite"};
        
        return allowedExtensions.contains(extension);
    } catch (const std::exception &e) {
        handleException(e, "isValidFilePath");
        return false;
    }
}

bool AnalysisScene::isValidPythonPath(const QString &pythonPath)
{
    try {
        QFileInfo pythonInfo(pythonPath);
        
        // Check for path traversal
        if (pythonPath.contains("..") || pythonPath.contains("~")) {
            return false;
        }
        
        // Check if it's a valid executable
        if (!pythonInfo.exists() || !pythonInfo.isExecutable()) {
            return false;
        }
        
        // Check file name for security
        QString fileName = pythonInfo.fileName().toLower();
        return fileName.contains("python") || fileName == "python.exe";
    } catch (const std::exception &e) {
        handleException(e, "isValidPythonPath");
        return false;
    }
}

void AnalysisScene::handleException(const std::exception &e, const QString &context)
{
    QString errorMsg = QString("Ошибка в %1: %2").arg(context).arg(e.what());
    appendLogMessage("❌ " + errorMsg);
    QMessageBox::critical(this, "Критическая ошибка", errorMsg);
}

void AnalysisScene::stopAnalysis()
{
    if (pythonProcess && pythonProcess->state() == QProcess::Running) {
        pythonProcess->terminate();
        if (!pythonProcess->waitForFinished(2000)) {
            pythonProcess->kill();
        }
    }
    progressBar->setVisible(false);
}

void AnalysisScene::appendLogMessage(const QString &message)
{
    QString trimmed = message.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    if (logOutput) {
        logOutput->append(trimmed);
    }
    emit analysisLogMessage(trimmed);
}

void AnalysisScene::appendLogMessages(const QStringList &messages)
{
    for (const QString &msg : messages) {
        appendLogMessage(msg);
    }
}

void AnalysisScene::handlePythonOutput()
{
    qDebug() << "[AnalysisScene] handlePythonOutput: получен вывод от Python процесса";
    if (!pythonProcess || pythonProcess->state() != QProcess::Running) {
        qDebug() << "[AnalysisScene] handlePythonOutput: процесс не запущен, состояние:" << (pythonProcess ? pythonProcess->state() : -1);
        return;
    }
    
    QByteArray output = pythonProcess->readAllStandardOutput();
    qDebug() << "[AnalysisScene] handlePythonOutput: получено новых данных:" << output.size() << "байт";
    
    // Накапливаем вывод в буфере
    pythonOutputBuffer.append(output);
    qDebug() << "[AnalysisScene] handlePythonOutput: размер буфера после добавления:" << pythonOutputBuffer.size() << "байт";
    
    QString outputStr = QString::fromUtf8(output);
    if (!outputStr.trimmed().isEmpty()) {
        QStringList lines = outputStr.split(QRegularExpression("[\\r\\n]+"), Qt::SkipEmptyParts);
        QStringList formatted;
        formatted.reserve(lines.size());
        for (const QString &line : lines) {
            QString trimmed = line.trimmed();
            if (!trimmed.isEmpty()) {
                formatted.append("📤 " + trimmed);
            }
        }
        appendLogMessages(formatted);
    }
    
    // НЕ парсим JSON здесь - будем парсить после завершения процесса
    // Это позволит получить полный JSON, который может приходить частями
}

void AnalysisScene::handlePythonError()
{
    qDebug() << "[AnalysisScene] handlePythonError: получен вывод stderr от Python процесса";
    if (!pythonProcess || pythonProcess->state() != QProcess::Running) {
        qDebug() << "[AnalysisScene] handlePythonError: процесс не запущен, игнорируем";
        return;
    }
    
    QByteArray error = pythonProcess->readAllStandardError();
    QString errorStr = QString::fromUtf8(error);
    qDebug() << "[AnalysisScene] handlePythonError: размер ошибки:" << error.size() << "байт";
    qDebug() << "[AnalysisScene] handlePythonError: первые 500 символов:" << errorStr.left(500);
    
    if (!errorStr.trimmed().isEmpty()) {
        QStringList lines = errorStr.split(QRegularExpression("[\\r\\n]+"), Qt::SkipEmptyParts);
        QStringList formatted;
        formatted.reserve(lines.size());
        for (const QString &line : lines) {
            QString trimmed = line.trimmed();
            if (!trimmed.isEmpty()) {
                formatted.append("⚠️ " + trimmed);
            }
        }
        appendLogMessages(formatted);
    }
}

void AnalysisScene::onPythonFinished(int exitCode)
{
    qDebug() << "[AnalysisScene] onPythonFinished: Python процесс завершен, код выхода:" << exitCode;
    
    // Читаем весь оставшийся вывод (на случай если что-то осталось)
    QByteArray remainingOutput = pythonProcess->readAllStandardOutput();
    if (!remainingOutput.isEmpty()) {
        qDebug() << "[AnalysisScene] onPythonFinished: получен дополнительный вывод:" << remainingOutput.size() << "байт";
        pythonOutputBuffer.append(remainingOutput);
    }
    
    qDebug() << "[AnalysisScene] onPythonFinished: финальный размер буфера вывода:" << pythonOutputBuffer.size() << "байт";
    QString fullOutputStr = QString::fromUtf8(pythonOutputBuffer);
    qDebug() << "[AnalysisScene] onPythonFinished: первые 500 символов вывода:" << fullOutputStr.left(500);
    qDebug() << "[AnalysisScene] onPythonFinished: последние 500 символов вывода:" << fullOutputStr.right(500);
    
    if (progressBar) {
        progressBar->setVisible(false);
        qDebug() << "[AnalysisScene] onPythonFinished: прогресс-бар скрыт";
    }
    
    // Re-enable analyze button
    analyzeButton->setEnabled(true);
    analyzeButton->setText("🔄 Анализировать модель");
    qDebug() << "[AnalysisScene] onPythonFinished: кнопка анализа переактивирована";
    
    if (exitCode == 0) {
        qDebug() << "[AnalysisScene] onPythonFinished: ✓ процесс завершился успешно";
        
        // Теперь парсим JSON из полного буфера
        qDebug() << "[AnalysisScene] onPythonFinished: попытка парсинга JSON из полного буфера";
        QJsonParseError error;
        QJsonDocument doc;
        
        // Сначала пытаемся найти маркер начала JSON
        QString outputStr = QString::fromUtf8(pythonOutputBuffer);
        QString jsonStr;
        
        // Ищем маркер "Analysis completed" или просто первый '{'
        int jsonStart = -1;
        int markerPos = outputStr.indexOf("Analysis completed");
        if (markerPos >= 0) {
            // Ищем '{' после маркера
            jsonStart = outputStr.indexOf('{', markerPos);
            qDebug() << "[AnalysisScene] onPythonFinished: найден маркер 'Analysis completed' на позиции" << markerPos << ", JSON начинается на" << jsonStart;
        }
        
        if (jsonStart < 0) {
            // Если маркер не найден, ищем просто первый '{'
            jsonStart = outputStr.indexOf('{');
            qDebug() << "[AnalysisScene] onPythonFinished: маркер не найден, используем первый '{' на позиции" << jsonStart;
        }
        
        if (jsonStart >= 0) {
            // Находим последнюю закрывающую скобку
            int jsonEnd = outputStr.lastIndexOf('}');
            if (jsonEnd > jsonStart) {
                jsonStr = outputStr.mid(jsonStart, jsonEnd - jsonStart + 1);
                qDebug() << "[AnalysisScene] onPythonFinished: извлечен JSON фрагмент, размер:" << jsonStr.size() << "символов";
                
                // Очищаем JSON от возможных проблемных символов в начале/конце
                jsonStr = jsonStr.trimmed();
                
                // Убираем возможные управляющие символы в начале
                while (!jsonStr.isEmpty() && (jsonStr[0] == '\r' || jsonStr[0] == '\n' || jsonStr[0] == ' ')) {
                    jsonStr = jsonStr.mid(1);
                }
                
                // Заменяем NaN, Infinity, -Infinity на null перед парсингом (JSON не поддерживает эти значения)
                QString cleanedJson = jsonStr;
                // Заменяем : NaN на : null (с учетом пробелов)
                cleanedJson.replace(QRegularExpression(":\\s*NaN\\b"), ": null");
                cleanedJson.replace(QRegularExpression(":\\s*Infinity\\b"), ": null");
                cleanedJson.replace(QRegularExpression(":\\s*-Infinity\\b"), ": null");
                // Также заменяем NaN в массивах и других местах
                cleanedJson.replace(QRegularExpression(",\\s*NaN\\b"), ", null");
                cleanedJson.replace(QRegularExpression("\\[\\s*NaN\\b"), "[ null");
                
                // Пытаемся распарсить извлеченный JSON
                doc = QJsonDocument::fromJson(cleanedJson.toUtf8(), &error);
                if (error.error != QJsonParseError::NoError) {
                    qDebug() << "[AnalysisScene] onPythonFinished: ошибка парсинга извлеченного JSON:" << error.error << ", позиция:" << error.offset;
                    
                    // Пытаемся исправить проблемные символы вокруг позиции ошибки
                    if (error.offset > 0 && error.offset < jsonStr.size()) {
                        qDebug() << "[AnalysisScene] onPythonFinished: контекст вокруг ошибки (позиция" << error.offset << "):";
                        int start = qMax(0, error.offset - 100);
                        int end = qMin(jsonStr.size(), error.offset + 100);
                        QString context = jsonStr.mid(start, end - start);
                        qDebug() << "[AnalysisScene] onPythonFinished: контекст:" << context;
                        
                        // Пробуем несколько стратегий исправления
                        QString cleanedJson = jsonStr;
                        
                        // Стратегия 1: Заменяем NaN, Infinity, -Infinity на null (JSON не поддерживает эти значения)
                        cleanedJson.replace(QRegularExpression(":\\s*NaN\\b"), ": null");
                        cleanedJson.replace(QRegularExpression(":\\s*Infinity\\b"), ": null");
                        cleanedJson.replace(QRegularExpression(":\\s*-Infinity\\b"), ": null");
                        cleanedJson.replace(QRegularExpression(",\\s*NaN\\b"), ", null");
                        cleanedJson.replace(QRegularExpression("\\[\\s*NaN\\b"), "[ null");
                        
                        // Стратегия 2: Убираем возможные управляющие символы (кроме \n, \r, \t)
                        cleanedJson.replace(QRegularExpression("\\x00|\\x01|\\x02|\\x03|\\x04|\\x05|\\x06|\\x07|\\x08|\\x0B|\\x0C|\\x0E|\\x0F"), "");
                        
                        // Стратегия 3: Исправляем неправильно экранированные обратные слеши в путях
                        // Заменяем последовательности вида "C:\Users" на "C:\\Users" (но не трогаем уже экранированные)
                        cleanedJson.replace(QRegularExpression("([A-Za-z]:)\\\\([^\\\\\"nrt])"), "\\1\\\\\\\\\\2");
                        
                        // Стратегия 4: Убираем возможные невидимые символы Unicode
                        cleanedJson.remove(QChar(0x200B)); // Zero-width space
                        cleanedJson.remove(QChar(0x200C)); // Zero-width non-joiner
                        cleanedJson.remove(QChar(0x200D)); // Zero-width joiner
                        cleanedJson.remove(QChar(0xFEFF)); // Zero-width no-break space
                        
                        QJsonParseError error2;
                        doc = QJsonDocument::fromJson(cleanedJson.toUtf8(), &error2);
                        if (error2.error == QJsonParseError::NoError) {
                            qDebug() << "[AnalysisScene] onPythonFinished: ✓ JSON успешно распарсен после очистки";
                            jsonStr = cleanedJson;
                            error = error2;
                        } else {
                            qDebug() << "[AnalysisScene] onPythonFinished: очистка не помогла, ошибка осталась:" << error2.error << "на позиции" << error2.offset;
                        }
                    }
                } else {
                    qDebug() << "[AnalysisScene] onPythonFinished: ✓ JSON успешно распарсен из извлеченного фрагмента";
                }
            }
        }
        
        // Если все еще не удалось, пытаемся парсить весь буфер
        if (error.error != QJsonParseError::NoError && doc.isEmpty()) {
            qDebug() << "[AnalysisScene] onPythonFinished: попытка парсинга всего буфера";
            doc = QJsonDocument::fromJson(pythonOutputBuffer, &error);
            qDebug() << "[AnalysisScene] onPythonFinished: ошибка парсинга всего буфера:" << error.error << ", позиция:" << error.offset;
        }
        
        if (error.error == QJsonParseError::NoError && doc.isObject()) {
            qDebug() << "[AnalysisScene] onPythonFinished: ✓ JSON успешно распарсен";
            QJsonObject modelData = doc.object();
            qDebug() << "[AnalysisScene] onPythonFinished: ключи в JSON:" << modelData.keys();
            
            // Сохраняем данные модели для передачи в упрощение
            currentModelData = modelData;
            
            // Отладочная информация
            qDebug() << "[AnalysisScene] onPythonFinished: данные модели сохранены:";
            qDebug() << "[AnalysisScene] onPythonFinished:   - Пусто ли:" << currentModelData.isEmpty();
            qDebug() << "[AnalysisScene] onPythonFinished:   - Ключи:" << currentModelData.keys();
            if (currentModelData.contains("layers")) {
                qDebug() << "[AnalysisScene] onPythonFinished:   - Количество слоев:" << currentModelData["layers"].toArray().size();
            }
            if (currentModelData.contains("training_data")) {
                qDebug() << "[AnalysisScene] onPythonFinished:   - Данные об обучающих данных присутствуют в JSON";
            }
            // Проверяем наличие точности
            if (currentModelData.contains("accuracy")) {
                QJsonValue accuracyValue = currentModelData.value("accuracy");
                if (accuracyValue.isDouble()) {
                    double accuracy = accuracyValue.toDouble();
                    qDebug() << "[AnalysisScene] onPythonFinished:   - ✓ Точность модели найдена:" << accuracy << "(" << (accuracy * 100.0) << "%)";
                } else if (accuracyValue.isNull()) {
                    qDebug() << "[AnalysisScene] onPythonFinished:   - ⚠️ Точность модели: null (не вычислена)";
                } else {
                    qDebug() << "[AnalysisScene] onPythonFinished:   - ⚠️ Точность модели: неверный тип" << accuracyValue.type();
                }
            } else {
                qDebug() << "[AnalysisScene] onPythonFinished:   - ⚠️ Точность модели отсутствует в данных";
            }
            
            if (modelData.contains("error")) {
                QString errorMsg = modelData["error"].toString();
                qDebug() << "[AnalysisScene] onPythonFinished: ОШИБКА в JSON ответе:" << errorMsg;
                appendLogMessage("❌ Ошибка: " + errorMsg);
                
                // Special handling for weights-only files
                if (modelData.contains("file_type") && modelData["file_type"].toString() == "weights_only") {
                    qDebug() << "[AnalysisScene] onPythonFinished: файл содержит только веса";
                    appendLogMessage("💡 Совет: " + modelData["suggestion"].toString());
                    appendLogMessage("📝 Для анализа нужен полный файл модели с архитектурой, а не только веса.");
                }
            } else {
                qDebug() << "[AnalysisScene] onPythonFinished: ✓ JSON успешно обработан, заполнение данных";
                
                // Populate all tabs with data
                populateModelTree(modelData);
                populateModelOverview(modelData);
                populateLayersTable(modelData);
                populateOptimizerInfo(modelData);
                populateMetricsInfo(modelData);
                populateWeightsInfo(modelData);
                populateWeightsTable(modelData);
                populateWeightsTree(modelData);
                
                // Обрабатываем данные об обучающих данных из JSON, если они есть
                qDebug() << "[AnalysisScene] onPythonFinished: обработка данных об обучающих данных";
                if (modelData.contains("training_data")) {
                    qDebug() << "[AnalysisScene] onPythonFinished: данные об обучающих данных найдены в JSON";
                    QJsonObject trainingData = modelData["training_data"].toObject();
                    currentTrainingData = trainingData;  // Сохраняем для передачи в SimplifyScene
                    appendLogMessage("📚 Обрабатываем данные об обучающих данных из анализа...");
                    populateTrainingDataFromJson(trainingData);
                } else if (modelData.contains("training_data_error")) {
                    QString errorMsg = modelData["training_data_error"].toString();
                    qDebug() << "[AnalysisScene] onPythonFinished: ошибка анализа обучающих данных:" << errorMsg;
                    currentTrainingData = QJsonObject();  // Очищаем при ошибке
                    appendLogMessage("⚠️ Ошибка анализа обучающих данных: " + errorMsg);
                    // Попробуем использовать локальное чтение как fallback
                    if (!trainingDataPath.isEmpty()) {
                        qDebug() << "[AnalysisScene] onPythonFinished: используем локальное чтение как fallback";
                        appendLogMessage("📖 Используем локальное чтение файла как альтернативу...");
                        populateTrainingDataInfo(trainingDataPath);
                    }
                } else if (!trainingDataPath.isEmpty()) {
                    qDebug() << "[AnalysisScene] onPythonFinished: данных в JSON нет, используем локальное чтение";
                    // Если данных в JSON нет, но путь указан, используем локальное чтение
                    appendLogMessage("📖 Данные об обучающих данных не найдены в JSON, используем локальное чтение...");
                    populateTrainingDataInfo(trainingDataPath);
                } else {
                    qDebug() << "[AnalysisScene] onPythonFinished: обучающие данные не указаны";
                }
                
                // Enable simplify button after successful analysis
                simplifyButton->setEnabled(true);
                
                // Switch to overview tab to show results
                mainTabWidget->setCurrentIndex(0);
                
                qDebug() << "[AnalysisScene] onPythonFinished: ✓ анализ завершен успешно";
                appendLogMessage("✅ Анализ завершен успешно!");
                appendLogMessage("📊 Данные загружены во все вкладки");
                appendLogMessage("🎯 Модель готова к оптимизации");
                
                emit analysisFinished();
            }
        } else {
            qDebug() << "[AnalysisScene] onPythonFinished: ОШИБКА - не удалось распарсить JSON ответ";
            appendLogMessage("❌ Ошибка: Не удалось распарсить JSON ответ от Python скрипта");
            appendLogMessage("💡 Возможно, скрипт вывел дополнительную информацию после JSON");
            appendLogMessage("📋 Полный вывод сохранен в логе");
        }
        
        appendLogMessage("🎉 Модель готова к упрощению");
    } else {
        qDebug() << "[AnalysisScene] onPythonFinished: ✗ процесс завершился с ошибкой, код:" << exitCode;
        appendLogMessage("❌ Процесс завершен с ошибкой (код: " + QString::number(exitCode) + ")");
        appendLogMessage("💡 Проверьте лог выше для подробностей");
    }
    qDebug() << "[AnalysisScene] onPythonFinished: ===== АНАЛИЗ ЗАВЕРШЕН =====";
    
    // Очищаем буфер после обработки
    pythonOutputBuffer.clear();
}

void AnalysisScene::populateModelTree(const QJsonObject &modelData)
{
    if (!modelTree) return;
    
    modelTree->clear();
    
    // Clean header section
    QTreeWidgetItem *basicHeader = new QTreeWidgetItem(modelTree.get());
    basicHeader->setText(0, "Основная информация");
    basicHeader->setText(1, "");
    basicHeader->setText(2, "");
    basicHeader->setText(3, "");
    basicHeader->setText(4, "");
    basicHeader->setText(5, "");
    // Minimalist colors
    QColor headerBg(245, 245, 245); // #f5f5f5
    QColor headerText(51, 51, 51);   // #333333
    basicHeader->setBackground(0, headerBg);
    basicHeader->setBackground(1, headerBg);
    basicHeader->setBackground(2, headerBg);
    basicHeader->setBackground(3, headerBg);
    basicHeader->setBackground(4, headerBg);
    basicHeader->setBackground(5, headerBg);
    basicHeader->setForeground(0, headerText);
    basicHeader->setForeground(1, headerText);
    basicHeader->setForeground(2, headerText);
    basicHeader->setForeground(3, headerText);
    basicHeader->setForeground(4, headerText);
    basicHeader->setForeground(5, headerText);
    
    // Model type and framework
    QTreeWidgetItem *modelTypeItem = new QTreeWidgetItem(modelTree.get());
    modelTypeItem->setText(0, "Тип модели");
    modelTypeItem->setText(1, modelData.contains("framework") ? modelData["framework"].toString() : "Keras");
    modelTypeItem->setText(2, "");
    modelTypeItem->setText(3, "");
    modelTypeItem->setText(4, "");
    modelTypeItem->setText(5, "");
    
    // TensorFlow version
    if (modelData.contains("tensorflow_version")) {
        QTreeWidgetItem *tfVersionItem = new QTreeWidgetItem(modelTree.get());
        tfVersionItem->setText(0, "Версия TensorFlow");
        tfVersionItem->setText(1, modelData["tensorflow_version"].toString());
        tfVersionItem->setText(2, "");
        tfVersionItem->setText(3, "");
        tfVersionItem->setText(4, "");
        tfVersionItem->setText(5, "");
    }
    
    // Total parameters
    QTreeWidgetItem *paramsItem = new QTreeWidgetItem(modelTree.get());
    paramsItem->setText(0, "Всего параметров");
    paramsItem->setText(1, QString::number(modelData["total_params"].toInt()));
    paramsItem->setText(2, QString::number(modelData["total_params"].toInt()));
    paramsItem->setText(3, "");
    paramsItem->setText(4, "");
    paramsItem->setText(5, "");
    
    // Trainable parameters
    QTreeWidgetItem *trainableItem = new QTreeWidgetItem(modelTree.get());
    trainableItem->setText(0, "Обучаемых параметров");
    trainableItem->setText(1, QString::number(modelData["trainable_params"].toInt()));
    trainableItem->setText(2, QString::number(modelData["trainable_params"].toInt()));
    trainableItem->setText(3, "");
    trainableItem->setText(4, "");
    trainableItem->setText(5, "");
    
    // Model size
    QTreeWidgetItem *sizeItem = new QTreeWidgetItem(modelTree.get());
    sizeItem->setText(0, "Размер файла");
    sizeItem->setText(1, QString::number(modelData["model_size_mb"].toDouble(), 'f', 2) + " MB");
    sizeItem->setText(2, "");
    sizeItem->setText(3, "");
    sizeItem->setText(4, "");
    sizeItem->setText(5, QString::number(modelData["model_size_mb"].toDouble(), 'f', 2) + " MB");
    
    // Expand the tree
    modelTree->expandAll();
}

bool AnalysisScene::checkPythonEnvironment()
{
    // Try to find venv in different locations
    QStringList possibleVenvPaths = {
        "venv",                                                    // Current directory
        "../venv",                                                 // Parent directory
        "../../venv",                                              // Two levels up
        QCoreApplication::applicationDirPath() + "/venv",          // Same as exe
        QCoreApplication::applicationDirPath() + "/../venv",       // Parent of exe
        QCoreApplication::applicationDirPath() + "/../../venv"     // Two levels up from exe
    };
    
    // Check virtual environments first
    for (const QString &venvPath : possibleVenvPaths) {
        if (QDir(venvPath).exists()) {
            QString pythonExe = QDir(venvPath).absoluteFilePath("Scripts/python.exe");
            if (QFile::exists(pythonExe)) {
                // Check if required packages are installed
                QProcess checkProcess;
                QStringList args;
                args << "-c" << "import tensorflow, keras, torch, numpy; print('OK')";
                checkProcess.start(pythonExe, args);
                checkProcess.waitForFinished(5000);
                
                if (checkProcess.exitCode() == 0) {
                    return true;
                }
            }
        }
    }
    
    // Check system Python as fallback
    QProcess checkProcess;
    QStringList args;
    args << "-c" << "import tensorflow, keras, torch, numpy; print('OK')";
    checkProcess.start("python", args);
    checkProcess.waitForFinished(5000);
    
    return checkProcess.exitCode() == 0;
}

void AnalysisScene::showDependencyError()
{
    QMessageBox msgBox;
    msgBox.setWindowTitle("Отсутствуют зависимости");
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setText("Для работы программы необходимо установить Python и библиотеки машинного обучения.");
    
    QString detailedText = 
        "Требования:\n\n"
        "1. Python 3.7+ (скачать с https://python.org)\n"
        "2. Библиотеки: tensorflow, keras, torch, numpy\n\n"
        "Способы установки:\n\n"
        "БЫСТРЫЙ СПОСОБ:\n"
        "Запустите файл install_dependencies.bat\n\n"
        "РУЧНАЯ УСТАНОВКА:\n"
        "1. Откройте командную строку в папке программы\n"
        "2. Выполните команды:\n"
        "   python -m venv venv\n"
        "   venv\\Scripts\\activate\n"
        "   pip install -r requirements.txt\n\n"
        "ПОДРОБНАЯ ИНСТРУКЦИЯ:\n"
        "Смотрите файл INSTALL.md\n\n"
        "После установки перезапустите программу.";
    
    msgBox.setDetailedText(detailedText);
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.exec();
}

void AnalysisScene::populateModelOverview(const QJsonObject &modelData)
{
    if (!overviewTable) {
        return;
    }
    
    // Set up overview table
    overviewTable->setColumnCount(2);
    overviewTable->setHorizontalHeaderLabels(QStringList() << "Параметр" << "Значение");
    
    // Calculate number of rows needed
    int rowCount = 0;
    if (modelData.contains("model_name")) rowCount++;
    if (modelData.contains("framework")) rowCount++;
    if (modelData.contains("tensorflow_version")) rowCount++;
    if (modelData.contains("total_params")) rowCount++;
    if (modelData.contains("trainable_params")) rowCount++;
    if (modelData.contains("non_trainable_params")) rowCount++;
    if (modelData.contains("total_layers")) rowCount++;
    if (modelData.contains("total_neurons")) rowCount++;
    if (modelData.contains("model_size_mb")) rowCount++;
    if (modelData.contains("accuracy")) rowCount++;  // Точность модели
    
    overviewTable->setRowCount(rowCount);
    
    int currentRow = 0;
    
    // Model information
    if (modelData.contains("model_name")) {
        overviewTable->setItem(currentRow, 0, new QTableWidgetItem("Название модели"));
        overviewTable->setItem(currentRow, 1, new QTableWidgetItem(modelData["model_name"].toString()));
        currentRow++;
    }
    
    if (modelData.contains("framework")) {
        overviewTable->setItem(currentRow, 0, new QTableWidgetItem("Фреймворк"));
        overviewTable->setItem(currentRow, 1, new QTableWidgetItem(modelData["framework"].toString()));
        currentRow++;
    }
    
    if (modelData.contains("tensorflow_version")) {
        overviewTable->setItem(currentRow, 0, new QTableWidgetItem("Версия TensorFlow"));
        overviewTable->setItem(currentRow, 1, new QTableWidgetItem(modelData["tensorflow_version"].toString()));
        currentRow++;
    }
    
    // Statistics
    if (modelData.contains("total_params")) {
        overviewTable->setItem(currentRow, 0, new QTableWidgetItem("Всего параметров"));
        overviewTable->setItem(currentRow, 1, new QTableWidgetItem(QString::number(modelData["total_params"].toInt())));
        currentRow++;
    }
    
    if (modelData.contains("trainable_params")) {
        overviewTable->setItem(currentRow, 0, new QTableWidgetItem("Обучаемых параметров"));
        overviewTable->setItem(currentRow, 1, new QTableWidgetItem(QString::number(modelData["trainable_params"].toInt())));
        currentRow++;
    }
    
    if (modelData.contains("non_trainable_params")) {
        overviewTable->setItem(currentRow, 0, new QTableWidgetItem("НЕ обучаемых параметров"));
        overviewTable->setItem(currentRow, 1, new QTableWidgetItem(QString::number(modelData["non_trainable_params"].toInt())));
        currentRow++;
    }
    
    if (modelData.contains("total_layers")) {
        overviewTable->setItem(currentRow, 0, new QTableWidgetItem("Всего слоев"));
        overviewTable->setItem(currentRow, 1, new QTableWidgetItem(QString::number(modelData["total_layers"].toInt())));
        currentRow++;
    }
    
    if (modelData.contains("total_neurons")) {
        overviewTable->setItem(currentRow, 0, new QTableWidgetItem("Всего нейронов"));
        overviewTable->setItem(currentRow, 1, new QTableWidgetItem(QString::number(modelData["total_neurons"].toInt())));
        currentRow++;
    }
    
    if (modelData.contains("model_size_mb")) {
        overviewTable->setItem(currentRow, 0, new QTableWidgetItem("Размер файла"));
        overviewTable->setItem(currentRow, 1, new QTableWidgetItem(QString::number(modelData["model_size_mb"].toDouble(), 'f', 2) + " MB"));
        currentRow++;
    }
    
    // Accuracy
    if (modelData.contains("accuracy")) {
        QJsonValue accuracyValue = modelData.value("accuracy");
        QString accuracyText;
        if (accuracyValue.isDouble()) {
            double accuracy = accuracyValue.toDouble();
            accuracyText = QString::number(accuracy * 100.0, 'f', 2) + "%";
        } else if (accuracyValue.isNull()) {
            accuracyText = "Не вычислена";
        } else {
            accuracyText = "Недоступна";
        }
        overviewTable->setItem(currentRow, 0, new QTableWidgetItem("Точность модели"));
        overviewTable->setItem(currentRow, 1, new QTableWidgetItem(accuracyText));
        currentRow++;
    }
    
    // Resize columns to content
    overviewTable->resizeColumnsToContents();
}

void AnalysisScene::populateLayersTable(const QJsonObject &modelData)
{
    if (!layersTable || !modelData.contains("layers")) return;
    
    QJsonArray layers = modelData["layers"].toArray();
    
    // Set up table
    layersTable->setColumnCount(8);
    layersTable->setHorizontalHeaderLabels(QStringList() 
        << "№" << "Название" << "Тип" << "Активация" 
        << "Параметры" << "Нейроны" << "Вход" << "Выход");
    
    layersTable->setRowCount(layers.size());
    
    for (int i = 0; i < layers.size(); ++i) {
        QJsonObject layer = layers[i].toObject();
        
        layersTable->setItem(i, 0, new QTableWidgetItem(QString::number(i)));
        layersTable->setItem(i, 1, new QTableWidgetItem(layer["name"].toString()));
        layersTable->setItem(i, 2, new QTableWidgetItem(layer["type"].toString()));
        layersTable->setItem(i, 3, new QTableWidgetItem(layer["activation"].toString()));
        layersTable->setItem(i, 4, new QTableWidgetItem(QString::number(layer["params"].toInt())));
        layersTable->setItem(i, 5, new QTableWidgetItem(QString::number(layer["neurons"].toInt())));
        layersTable->setItem(i, 6, new QTableWidgetItem(layer["input_shape"].toString()));
        layersTable->setItem(i, 7, new QTableWidgetItem(layer["output_shape"].toString()));
    }
    
    // Resize columns to content
    layersTable->resizeColumnsToContents();
}

void AnalysisScene::populateOptimizerInfo(const QJsonObject &modelData)
{
    if (!optimizerTable || !modelData.contains("optimizer_info")) return;
    
    QJsonObject optimizerInfo = modelData["optimizer_info"].toObject();
    if (optimizerInfo.isEmpty()) return;
    
    optimizerTable->setColumnCount(2);
    optimizerTable->setHorizontalHeaderLabels(QStringList() << "Параметр" << "Значение");
    optimizerTable->setRowCount(2);
    
    optimizerTable->setItem(0, 0, new QTableWidgetItem("Название"));
    optimizerTable->setItem(0, 1, new QTableWidgetItem(optimizerInfo["name"].toString()));
    
    if (optimizerInfo.contains("learning_rate")) {
        optimizerTable->setItem(1, 0, new QTableWidgetItem("Скорость обучения"));
        optimizerTable->setItem(1, 1, new QTableWidgetItem(
            QString::number(optimizerInfo["learning_rate"].toDouble(), 'g', 6)));
    }
    
    optimizerTable->resizeColumnsToContents();
}

void AnalysisScene::populateMetricsInfo(const QJsonObject &modelData)
{
    if (!metricsTable || !modelData.contains("metrics_info")) return;
    
    QJsonArray metrics = modelData["metrics_info"].toArray();
    if (metrics.isEmpty()) return;
    
    metricsTable->setColumnCount(1);
    metricsTable->setHorizontalHeaderLabels(QStringList() << "Метрики");
    metricsTable->setRowCount(metrics.size());
    
    for (int i = 0; i < metrics.size(); ++i) {
        metricsTable->setItem(i, 0, new QTableWidgetItem(metrics[i].toString()));
    }
    
    metricsTable->resizeColumnsToContents();
}

void AnalysisScene::populateWeightsInfo(const QJsonObject &modelData)
{
    if (!weightsTextEdit || !modelData.contains("layers")) return;
    
    QJsonArray layers = modelData["layers"].toArray();
    QString weightsInfo;
    
    weightsInfo += "=== ИНФОРМАЦИЯ О ВЕСАХ МОДЕЛИ ===\n\n";
    
    for (int i = 0; i < layers.size(); ++i) {
        QJsonObject layer = layers[i].toObject();
        
        if (layer.contains("weights_info") && !layer["weights_info"].toObject().isEmpty()) {
            QJsonObject weightsInfoObj = layer["weights_info"].toObject();
            
            weightsInfo += QString("Слой %1: %2\n").arg(i).arg(layer["name"].toString());
            weightsInfo += QString("Тип: %1\n").arg(layer["type"].toString());
            
            if (weightsInfoObj.contains("weights_shape")) {
                weightsInfo += QString("Форма весов: %1\n").arg(weightsInfoObj["weights_shape"].toString());
            }
            if (weightsInfoObj.contains("weights_range")) {
                weightsInfo += QString("Диапазон весов: %1\n").arg(weightsInfoObj["weights_range"].toString());
            }
            if (weightsInfoObj.contains("weights_mean")) {
                weightsInfo += QString("Среднее весов: %1\n").arg(
                    QString::number(weightsInfoObj["weights_mean"].toDouble(), 'f', 4));
            }
            if (weightsInfoObj.contains("weights_std")) {
                weightsInfo += QString("Стд. отклонение весов: %1\n").arg(
                    QString::number(weightsInfoObj["weights_std"].toDouble(), 'f', 4));
            }
            if (weightsInfoObj.contains("weights_zeros")) {
                int zeros = weightsInfoObj["weights_zeros"].toInt();
                int total = weightsInfoObj["weights_size"].toInt();
                if (total > 0) {
                    weightsInfo += QString("Нулевые веса: %1 из %2 (%3%)\n").arg(zeros).arg(total).arg(
                        QString::number(100.0 * zeros / total, 'f', 1));
                }
            }
            if (weightsInfoObj.contains("biases_shape")) {
                weightsInfo += QString("Форма смещений: %1\n").arg(weightsInfoObj["biases_shape"].toString());
            }
            if (weightsInfoObj.contains("biases_range")) {
                weightsInfo += QString("Диапазон смещений: %1\n").arg(weightsInfoObj["biases_range"].toString());
            }
            if (weightsInfoObj.contains("biases_mean")) {
                weightsInfo += QString("Среднее смещений: %1\n").arg(
                    QString::number(weightsInfoObj["biases_mean"].toDouble(), 'f', 4));
            }
            if (weightsInfoObj.contains("biases_std")) {
                weightsInfo += QString("Стд. отклонение смещений: %1\n").arg(
                    QString::number(weightsInfoObj["biases_std"].toDouble(), 'f', 4));
            }
            
            weightsInfo += "\n" + QString("-").repeated(50) + "\n\n";
        }
    }
    
    if (weightsInfo.isEmpty()) {
        weightsInfo = "Информация о весах недоступна для данной модели.";
    }
    
    weightsTextEdit->setPlainText(weightsInfo);
}

void AnalysisScene::populateWeightsTable(const QJsonObject &modelData)
{
    if (!weightsTable || !modelData.contains("layers")) return;
    
    QJsonArray layers = modelData["layers"].toArray();
    
    // Count layers with weights
    int layersWithWeights = 0;
    for (int i = 0; i < layers.size(); ++i) {
        QJsonObject layer = layers[i].toObject();
        if (layer.contains("weights_info") && !layer["weights_info"].toObject().isEmpty()) {
            layersWithWeights++;
        }
    }
    
    if (layersWithWeights == 0) {
        weightsTable->setRowCount(1);
        weightsTable->setColumnCount(1);
        weightsTable->setHorizontalHeaderLabels(QStringList() << "Информация");
        weightsTable->setItem(0, 0, new QTableWidgetItem("Информация о весах недоступна для данной модели"));
        return;
    }
    
    // Set up table
    weightsTable->setColumnCount(6);
    weightsTable->setHorizontalHeaderLabels(QStringList() 
        << "Слой" << "Тип весов" << "Форма" << "Диапазон" << "Параметры" << "Статистика");
    
    weightsTable->setRowCount(layersWithWeights);
    
    int row = 0;
    for (int i = 0; i < layers.size(); ++i) {
        QJsonObject layer = layers[i].toObject();
        
        if (layer.contains("weights_info") && !layer["weights_info"].toObject().isEmpty()) {
            QJsonObject weightsInfo = layer["weights_info"].toObject();
            
            // Layer name and type
            weightsTable->setItem(row, 0, new QTableWidgetItem(
                QString("%1 (%2)").arg(layer["name"].toString()).arg(layer["type"].toString())));
            
            // Weights type
            QString weightsType = "Веса";
            if (weightsInfo.contains("biases_shape")) {
                weightsType += " + Смещения";
            }
            weightsTable->setItem(row, 1, new QTableWidgetItem(weightsType));
            
            // Shape information
            QString shapeInfo;
            if (weightsInfo.contains("weights_shape")) {
                shapeInfo = weightsInfo["weights_shape"].toString();
            }
            if (weightsInfo.contains("biases_shape")) {
                if (!shapeInfo.isEmpty()) shapeInfo += "\n";
                shapeInfo += "Смещения: " + weightsInfo["biases_shape"].toString();
            }
            weightsTable->setItem(row, 2, new QTableWidgetItem(shapeInfo));
            
            // Range information
            QString rangeInfo;
            if (weightsInfo.contains("weights_range")) {
                rangeInfo = weightsInfo["weights_range"].toString();
            }
            if (weightsInfo.contains("biases_range")) {
                if (!rangeInfo.isEmpty()) rangeInfo += "\n";
                rangeInfo += "Смещения: " + weightsInfo["biases_range"].toString();
            }
            weightsTable->setItem(row, 3, new QTableWidgetItem(rangeInfo));
            
            // Parameters count
            weightsTable->setItem(row, 4, new QTableWidgetItem(QString::number(layer["params"].toInt())));
            
            // Statistics
            QString stats = QString("Нейроны: %1").arg(layer["neurons"].toInt());
            if (layer["params"].toInt() > 0) {
                stats += QString("\nПлотность: %1").arg(
                    QString::number(double(layer["neurons"].toInt()) / layer["params"].toInt(), 'f', 3));
            }
            
            // Add detailed weight statistics if available
            if (weightsInfo.contains("weights_mean")) {
                stats += QString("\nСреднее весов: %1").arg(
                    QString::number(weightsInfo["weights_mean"].toDouble(), 'f', 4));
            }
            if (weightsInfo.contains("weights_std")) {
                stats += QString("\nСтд. откл.: %1").arg(
                    QString::number(weightsInfo["weights_std"].toDouble(), 'f', 4));
            }
            if (weightsInfo.contains("weights_zeros")) {
                int zeros = weightsInfo["weights_zeros"].toInt();
                int total = weightsInfo["weights_size"].toInt();
                if (total > 0) {
                    stats += QString("\nНули: %1 (%2%)").arg(zeros).arg(
                        QString::number(100.0 * zeros / total, 'f', 1));
                }
            }
            
            weightsTable->setItem(row, 5, new QTableWidgetItem(stats));
            
            row++;
        }
    }
    
    // Resize columns to content
    weightsTable->resizeColumnsToContents();
}

void AnalysisScene::populateWeightsTree(const QJsonObject &modelData)
{
    if (!weightsTree || !modelData.contains("layers")) return;
    
    weightsTree->clear();
    
    QJsonArray layers = modelData["layers"].toArray();
    
    for (int i = 0; i < layers.size(); ++i) {
        QJsonObject layer = layers[i].toObject();
        
        if (layer.contains("weights_info") && !layer["weights_info"].toObject().isEmpty()) {
            QJsonObject weightsInfo = layer["weights_info"].toObject();
            
            // Create main layer item
            QTreeWidgetItem *layerItem = new QTreeWidgetItem(weightsTree.get());
            layerItem->setText(0, QString("Слой %1: %2").arg(i).arg(layer["name"].toString()));
            layerItem->setText(1, layer["type"].toString());
            layerItem->setText(2, QString::number(layer["params"].toInt()));
            layerItem->setText(3, QString::number(layer["neurons"].toInt()));
            layerItem->setText(4, QString("Параметры: %1").arg(layer["params"].toInt()));
            
            // Add weights child
            if (weightsInfo.contains("weights_shape")) {
                QTreeWidgetItem *weightsItem = new QTreeWidgetItem(layerItem);
                weightsItem->setText(0, "Веса");
                weightsItem->setText(1, "Матрица весов");
                weightsItem->setText(2, weightsInfo["weights_shape"].toString());
                weightsItem->setText(3, weightsInfo["weights_range"].toString());
                weightsItem->setText(4, QString("Форма: %1").arg(weightsInfo["weights_shape"].toString()));
            }
            
            // Add biases child
            if (weightsInfo.contains("biases_shape")) {
                QTreeWidgetItem *biasesItem = new QTreeWidgetItem(layerItem);
                biasesItem->setText(0, "Смещения");
                biasesItem->setText(1, "Вектор смещений");
                biasesItem->setText(2, weightsInfo["biases_shape"].toString());
                biasesItem->setText(3, weightsInfo["biases_range"].toString());
                biasesItem->setText(4, QString("Форма: %1").arg(weightsInfo["biases_shape"].toString()));
            }
            
            // Expand the layer item
            layerItem->setExpanded(true);
        }
    }
    
    // Expand all items by default
    weightsTree->expandAll();
}

QJsonObject AnalysisScene::getModelData() const
{
    return currentModelData;
}

QJsonObject AnalysisScene::getTrainingData() const
{
    return currentTrainingData;
}

