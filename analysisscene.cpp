#include "analysisscene.h"
#include <QFileInfo>
#include <QMessageBox>
#include <QJsonParseError>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDir>
#include <QCoreApplication>

AnalysisScene::AnalysisScene(QWidget *parent)
    : QWidget(parent)
    , pythonProcess(nullptr)
    , overviewTable(nullptr)
    , layersTable(nullptr)
    , optimizerTable(nullptr)
    , metricsTable(nullptr)
    , weightsTextEdit(nullptr)
    , weightsTable(nullptr)
    , weightsTree(nullptr)
    , modelTree(nullptr)
    , mainTabWidget(nullptr)
    , analyzeButton(nullptr)
    , simplifyButton(nullptr)
    , backButton(nullptr)
    , modelTitleLabel(nullptr)
    , modelPathLabel(nullptr)
    , logOutput(nullptr)
    , progressBar(nullptr)
{
    setupUI();
    
    // Initialize Python process
    pythonProcess = new QProcess(this);
    connect(pythonProcess, &QProcess::readyReadStandardOutput, this, &AnalysisScene::handlePythonOutput);
    connect(pythonProcess, &QProcess::readyReadStandardError, this, &AnalysisScene::handlePythonError);
    connect(pythonProcess, &QProcess::finished, this, &AnalysisScene::onPythonFinished);
}

AnalysisScene::~AnalysisScene()
{
    if (pythonProcess && pythonProcess->state() == QProcess::Running) {
        pythonProcess->terminate();
        if (!pythonProcess->waitForFinished(3000)) {
            pythonProcess->kill();
        }
    }
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
    
    modelTitleLabel = new QLabel("Анализ модели");
    modelTitleLabel->setStyleSheet(R"(
        font-size: 24px; 
        font-weight: 600; 
        color: #333333;
        padding: 8px 0;
    )");
    headerLayout->addWidget(modelTitleLabel);
    
    headerLayout->addStretch();
    
    mainLayout->addLayout(headerLayout);

    // Simple model path
    modelPathLabel = new QLabel();
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
    mainLayout->addWidget(modelPathLabel);

    // Clean progress bar
    progressBar = new QProgressBar();
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
    mainLayout->addWidget(progressBar);

    // Minimalist tab widget
    mainTabWidget = new QTabWidget();
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
    overviewTable = new QTableWidget();
    overviewTable->setAlternatingRowColors(true);
    overviewTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    overviewTable->setStyleSheet(tableStyle);
    mainTabWidget->addTab(overviewTable, "Обзор");

    // Layers table
    layersTable = new QTableWidget();
    layersTable->setAlternatingRowColors(true);
    layersTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    layersTable->setSortingEnabled(true);
    layersTable->setStyleSheet(tableStyle);
    mainTabWidget->addTab(layersTable, "Слои");

    // Optimizer tab
    optimizerTable = new QTableWidget();
    optimizerTable->setAlternatingRowColors(true);
    optimizerTable->setStyleSheet(tableStyle);
    mainTabWidget->addTab(optimizerTable, "Оптимизатор");

    // Metrics tab
    metricsTable = new QTableWidget();
    metricsTable->setAlternatingRowColors(true);
    metricsTable->setStyleSheet(tableStyle);
    mainTabWidget->addTab(metricsTable, "Метрики");

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
    weightsTextEdit = new QTextEdit();
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
    weightsTabWidget->addTab(weightsTextEdit, "Текст");
    
    // Table view for structured weights info
    weightsTable = new QTableWidget();
    weightsTable->setAlternatingRowColors(true);
    weightsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    weightsTable->setSortingEnabled(true);
    weightsTable->setStyleSheet(tableStyle);
    weightsTabWidget->addTab(weightsTable, "Таблица");
    
    // Tree view for hierarchical weights info
    weightsTree = new QTreeWidget();
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
    weightsTabWidget->addTab(weightsTree, "Дерево");
    
    weightsLayout->addWidget(weightsTabWidget);
    mainTabWidget->addTab(weightsWidget, "Веса");

    // Tree view tab
    modelTree = new QTreeWidget();
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
    mainTabWidget->addTab(modelTree, "Дерево");

    mainLayout->addWidget(mainTabWidget);

    // Minimalist buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(12);
    
    analyzeButton = new QPushButton("Анализировать модель");
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
    buttonLayout->addWidget(analyzeButton);
    
    buttonLayout->addStretch();
    
    simplifyButton = new QPushButton("Упростить модель");
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
    buttonLayout->addWidget(simplifyButton);
    
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
    
    logOutput = new QTextEdit();
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
    mainLayout->addWidget(logOutput);

    // Connect signals
    connect(backButton, &QPushButton::clicked, this, &AnalysisScene::backRequested);
    connect(analyzeButton, &QPushButton::clicked, this, &AnalysisScene::startAnalysis);
    connect(simplifyButton, &QPushButton::clicked, this, &AnalysisScene::simplifyRequested);
}

void AnalysisScene::setModelPath(const QString &filePath)
{
    currentFilePath = filePath;
    QFileInfo fileInfo(filePath);
    modelTitleLabel->setText("Анализ модели: " + fileInfo.fileName());
    modelPathLabel->setText("📁 " + filePath);
}

void AnalysisScene::clearData()
{
    modelTree->clear();
    logOutput->clear();
    simplifyButton->setEnabled(false);
}

void AnalysisScene::startAnalysis()
{
    if (currentFilePath.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Сначала выберите модель");
        return;
    }
    
    // Check Python environment first
    if (!checkPythonEnvironment()) {
        showDependencyError();
        return;
    }
    
    // Enhanced loading state
    logOutput->append("🔍 Начинаем анализ модели...");
    logOutput->append("📁 Путь к модели: " + currentFilePath);
    progressBar->setVisible(true);
    progressBar->setFormat("⏳ Анализируем модель... %p%");
    
    // Disable buttons during analysis
    analyzeButton->setEnabled(false);
    analyzeButton->setText("⏳ Анализируем...");
    simplifyButton->setEnabled(false);
    
    // Clear previous data
    clearData();
    
    // Check if virtual environment exists
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
    
    bool venvFound = false;
    for (const QString &path : possibleVenvPaths) {
        if (QDir(path).exists()) {
            QString venvPython = QDir(path).absoluteFilePath("Scripts/python.exe");
            if (QFile::exists(venvPython)) {
                pythonExe = venvPython;
                venvPath = path;
                venvFound = true;
                logOutput->append("🐍 Найдено виртуальное окружение: " + venvPath);
                logOutput->append("🐍 Используем Python: " + pythonExe);
                break;
            }
        }
    }
    
    if (!venvFound) {
        // Use system Python
        pythonExe = "python";
        logOutput->append("🐍 Виртуальное окружение не найдено, используем системный Python");
        logOutput->append("💡 Для создания venv запустите: install_dependencies.bat");
    }
    
    // Start Python analysis script - find it relative to executable
    QString scriptPath;
    QString exeDir = QCoreApplication::applicationDirPath();
    QString currentDir = QDir::currentPath();
    
    // Try different possible locations for the script
    QStringList possiblePaths = {
        exeDir + "/analyze_model.py",                    // Same directory as exe
        exeDir + "/../analyze_model.py",                 // Parent directory
        exeDir + "/../../analyze_model.py",              // Two levels up
        currentDir + "/analyze_model.py",                // Current working directory
        currentDir + "/../analyze_model.py",             // Parent of current directory
        "analyze_model.py"                               // In PATH or current directory
    };
    
    for (const QString &path : possiblePaths) {
        if (QFile::exists(path)) {
            scriptPath = path;
            break;
        }
    }
    
    if (scriptPath.isEmpty()) {
        logOutput->append("❌ Ошибка: Файл analyze_model.py не найден!");
        logOutput->append("Искали в следующих местах:");
        for (const QString &path : possiblePaths) {
            logOutput->append("  - " + path);
        }
        progressBar->setVisible(false);
        return;
    }
    
    QStringList args;
    args << scriptPath << currentFilePath;
    
    logOutput->append("▶️ Запускаем: " + pythonExe + " " + scriptPath + " " + currentFilePath);
    
    pythonProcess->start(pythonExe, args);
    
    if (!pythonProcess->waitForStarted(5000)) {
        logOutput->append("❌ Ошибка запуска Python процесса");
        progressBar->setVisible(false);
        QMessageBox::critical(this, "Ошибка", "Не удалось запустить Python. Убедитесь, что Python установлен и доступен в PATH.");
    }
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

void AnalysisScene::handlePythonOutput()
{
    if (pythonProcess->state() != QProcess::Running) {
        return;
    }
    
    QByteArray output = pythonProcess->readAllStandardOutput();
    QString outputStr = QString::fromUtf8(output);
    
    if (!outputStr.trimmed().isEmpty()) {
        logOutput->append("📤 " + outputStr.trimmed());
    }
    
    // Try to parse JSON output
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(output, &error);
    
    if (error.error == QJsonParseError::NoError && doc.isObject()) {
        QJsonObject modelData = doc.object();
        
        if (modelData.contains("error")) {
            QString errorMsg = modelData["error"].toString();
            logOutput->append("❌ Ошибка: " + errorMsg);
            
            // Special handling for weights-only files
            if (modelData.contains("file_type") && modelData["file_type"].toString() == "weights_only") {
                logOutput->append("💡 Совет: " + modelData["suggestion"].toString());
                logOutput->append("📝 Для анализа нужен полный файл модели с архитектурой, а не только веса.");
            }
        } else {
            // Populate all tabs with data
            populateModelTree(modelData);
            populateModelOverview(modelData);
            populateLayersTable(modelData);
            populateOptimizerInfo(modelData);
            populateMetricsInfo(modelData);
            populateWeightsInfo(modelData);
            populateWeightsTable(modelData);
            populateWeightsTree(modelData);
            
            logOutput->append("✅ Анализ завершен успешно!");
            logOutput->append("📊 Данные загружены во все вкладки");
            logOutput->append("🎯 Модель готова к оптимизации");
            
            // Enable simplify button after successful analysis
            simplifyButton->setEnabled(true);
            
            // Switch to overview tab to show results
            mainTabWidget->setCurrentIndex(0);
            
            emit analysisFinished();
        }
    }
}

void AnalysisScene::handlePythonError()
{
    if (pythonProcess->state() != QProcess::Running) {
        return;
    }
    
    QByteArray error = pythonProcess->readAllStandardError();
    QString errorStr = QString::fromUtf8(error);
    
    if (!errorStr.trimmed().isEmpty()) {
        logOutput->append("⚠️ " + errorStr.trimmed());
    }
}

void AnalysisScene::onPythonFinished(int exitCode)
{
    if (progressBar) {
        progressBar->setVisible(false);
    }
    
    // Re-enable analyze button
    analyzeButton->setEnabled(true);
    analyzeButton->setText("🔄 Анализировать модель");
    
    if (exitCode == 0) {
        logOutput->append("✅ Анализ завершен успешно!");
        logOutput->append("🎉 Модель готова к упрощению");
    } else {
        logOutput->append("❌ Процесс завершен с ошибкой (код: " + QString::number(exitCode) + ")");
        logOutput->append("💡 Проверьте лог выше для подробностей");
    }
}

void AnalysisScene::populateModelTree(const QJsonObject &modelData)
{
    if (!modelTree) return;
    
    modelTree->clear();
    
    // Clean header section
    QTreeWidgetItem *basicHeader = new QTreeWidgetItem(modelTree);
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
    QTreeWidgetItem *modelTypeItem = new QTreeWidgetItem(modelTree);
    modelTypeItem->setText(0, "Тип модели");
    modelTypeItem->setText(1, modelData.contains("framework") ? modelData["framework"].toString() : "Keras");
    modelTypeItem->setText(2, "");
    modelTypeItem->setText(3, "");
    modelTypeItem->setText(4, "");
    modelTypeItem->setText(5, "");
    
    // TensorFlow version
    if (modelData.contains("tensorflow_version")) {
        QTreeWidgetItem *tfVersionItem = new QTreeWidgetItem(modelTree);
        tfVersionItem->setText(0, "Версия TensorFlow");
        tfVersionItem->setText(1, modelData["tensorflow_version"].toString());
        tfVersionItem->setText(2, "");
        tfVersionItem->setText(3, "");
        tfVersionItem->setText(4, "");
        tfVersionItem->setText(5, "");
    }
    
    // Total parameters
    QTreeWidgetItem *paramsItem = new QTreeWidgetItem(modelTree);
    paramsItem->setText(0, "Всего параметров");
    paramsItem->setText(1, QString::number(modelData["total_params"].toInt()));
    paramsItem->setText(2, QString::number(modelData["total_params"].toInt()));
    paramsItem->setText(3, "");
    paramsItem->setText(4, "");
    paramsItem->setText(5, "");
    
    // Trainable parameters
    QTreeWidgetItem *trainableItem = new QTreeWidgetItem(modelTree);
    trainableItem->setText(0, "Обучаемых параметров");
    trainableItem->setText(1, QString::number(modelData["trainable_params"].toInt()));
    trainableItem->setText(2, QString::number(modelData["trainable_params"].toInt()));
    trainableItem->setText(3, "");
    trainableItem->setText(4, "");
    trainableItem->setText(5, "");
    
    // Model size
    QTreeWidgetItem *sizeItem = new QTreeWidgetItem(modelTree);
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
            QTreeWidgetItem *layerItem = new QTreeWidgetItem(weightsTree);
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

