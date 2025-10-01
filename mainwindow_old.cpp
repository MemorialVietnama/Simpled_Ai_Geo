#include "mainwindow.h"
#include <QFileDialog>
#include <QHBoxLayout>
#include <QApplication>
#include <QMimeData>
#include <QFileInfo>
#include <QMessageBox>
#include <QJsonParseError>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    // Set window properties
    setWindowTitle("Simpled-Ai - AI Model Optimizer");
    setMinimumSize(800, 600);
    setAcceptDrops(true);
    
    // Apply modern styling with animations
    setStyleSheet(R"(
        QMainWindow {
            background-color: #f8f9fa;
        }
        QWidget {
            font-family: 'Segoe UI', Arial, sans-serif;
            font-size: 14px;
        }
        QPushButton {
            background-color: #4a90e2;
            color: white;
            border: none;
            border-radius: 8px;
            padding: 12px 24px;
            font-weight: 600;
            font-size: 16px;
        }
        QPushButton:hover {
            background-color: #357abd;
        }
        QPushButton:pressed {
            background-color: #2c5aa0;
        }
        QPushButton#simplifyButton {
            background-color: #4a90e2;
        }
        QPushButton#simplifyButton:hover {
            background-color: #357abd;
        }
        QPushButton#backButton {
            background-color: #4a90e2;
        }
        QPushButton#backButton:hover {
            background-color: #357abd;
        }
        QLabel {
            color: #495057;
        }
        QFrame#dropArea {
            border: 3px dashed #dee2e6;
            border-radius: 12px;
            background-color: #ffffff;
            margin: 20px;
        }
        QFrame#dropArea:hover {
            border-color: #007bff;
            background-color: #f8f9ff;
        }
        QTreeWidget {
            border: 1px solid #dee2e6;
            border-radius: 8px;
            background-color: white;
            alternate-background-color: #f8f9fa;
            color: black;
        }
        QTreeWidget::item {
            padding: 8px;
            border-bottom: 1px solid #e9ecef;
            color: black;
        }
        QTreeWidget::item:hover {
            background-color: #f0f8ff;
        }
        QTreeWidget::item:selected {
            background-color: #4a90e2;
            color: white;
        }
        QTextEdit {
            border: 1px solid #dee2e6;
            border-radius: 8px;
            background-color: white;
            font-family: 'Consolas', 'Monaco', monospace;
            color: black;
        }
        QTextEdit:focus {
            border-color: #4a90e2;
        }
        QProgressBar {
            border: 1px solid #dee2e6;
            border-radius: 8px;
            background-color: #e9ecef;
        }
        QProgressBar::chunk {
            background-color: #007bff;
            border-radius: 7px;
        }
    )");

    // Initialize Python process
    pythonProcess = new QProcess(this);
    connect(pythonProcess, &QProcess::readyReadStandardOutput, this, &MainWindow::handlePythonOutput);
    connect(pythonProcess, &QProcess::readyReadStandardError, this, &MainWindow::handlePythonError);
    connect(pythonProcess, &QProcess::finished, this, [this](int exitCode) {
        if (progressBar) {
            progressBar->setVisible(false);
        }
        logOutput->append("🏁 Процесс завершен с кодом: " + QString::number(exitCode));
    });

    // Create stacked widget for different views
    stackedWidget = new QStackedWidget(this);
    setCentralWidget(stackedWidget);

    // Create views
    createFileSelectionView();
    createLoaderView();
    createAnalysisView();

    // Start with file selection view
    stackedWidget->setCurrentWidget(fileSelectionWidget);
}

MainWindow::~MainWindow() 
{
    // Properly terminate Python process if running
    if (pythonProcess && pythonProcess->state() == QProcess::Running) {
        pythonProcess->terminate();
        if (!pythonProcess->waitForFinished(3000)) {
            pythonProcess->kill();
        }
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // Terminate Python process before closing
    if (pythonProcess && pythonProcess->state() == QProcess::Running) {
        pythonProcess->terminate();
        if (!pythonProcess->waitForFinished(2000)) {
            pythonProcess->kill();
        }
    }
    
    // Accept the close event
    event->accept();
}

void MainWindow::createFileSelectionView()
{
    fileSelectionWidget = new QWidget();
    QVBoxLayout *mainLayout = new QVBoxLayout(fileSelectionWidget);
    mainLayout->setSpacing(20);
    mainLayout->setContentsMargins(30, 30, 30, 30);

    // Create title
    QLabel *titleLabel = new QLabel("Simpled-Ai");
    titleLabel->setStyleSheet("font-size: 28px; font-weight: bold; color: #212529;");
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);

    // Create subtitle
    QLabel *subtitleLabel = new QLabel("Загрузите модель для анализа и оптимизации");
    subtitleLabel->setStyleSheet("font-size: 16px; color: #6c757d; margin-bottom: 20px;");
    subtitleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(subtitleLabel);

    // Create drop area
    dropArea = new QFrame();
    dropArea->setObjectName("dropArea");
    dropArea->setMinimumHeight(150);
    dropArea->setMaximumHeight(200);
    
    QVBoxLayout *dropLayout = new QVBoxLayout(dropArea);
    dropLayout->setAlignment(Qt::AlignCenter);
    dropLayout->setSpacing(10);
    
    // Drop area icon
    QLabel *iconLabel = new QLabel("🧠");
    iconLabel->setStyleSheet("font-size: 40px;");
    iconLabel->setAlignment(Qt::AlignCenter);
    dropLayout->addWidget(iconLabel);
    
    // Drop area text
    dropAreaLabel = new QLabel("Перетащите модель сюда\nили");
    dropAreaLabel->setStyleSheet("font-size: 14px; color: #6c757d;");
    dropAreaLabel->setAlignment(Qt::AlignCenter);
    dropAreaLabel->setWordWrap(true);
    dropLayout->addWidget(dropAreaLabel);
    
    // Select file button
    selectFileButton = new QPushButton("Выбрать модель");
    selectFileButton->setMaximumWidth(180);
    selectFileButton->setMinimumHeight(40);
    dropLayout->addWidget(selectFileButton, 0, Qt::AlignCenter);
    
    mainLayout->addWidget(dropArea);

    // File info area
    filePathLabel = new QLabel("Модель не выбрана");
    filePathLabel->setStyleSheet("font-size: 12px; color: #6c757d; margin-top: 15px; padding: 10px; background-color: #e9ecef; border-radius: 6px;");
    filePathLabel->setAlignment(Qt::AlignCenter);
    filePathLabel->setWordWrap(true);
    filePathLabel->setMinimumHeight(60);
    mainLayout->addWidget(filePathLabel);

    // Analyze button (initially hidden)
    analyzeFileButton = new QPushButton("⚙️ Обработать");
    analyzeFileButton->setStyleSheet("font-size: 14px; padding: 12px 25px; margin-top: 15px;");
    analyzeFileButton->setVisible(false);
    analyzeFileButton->setMinimumHeight(45);
    mainLayout->addWidget(analyzeFileButton);

    // Add some spacing
    mainLayout->addStretch(1);

    // Connect signals
    connect(selectFileButton, &QPushButton::clicked, this, &MainWindow::selectFile);
    connect(analyzeFileButton, &QPushButton::clicked, this, &MainWindow::startAnalysis);
    
    // Add to stacked widget
    stackedWidget->addWidget(fileSelectionWidget);
}

void MainWindow::createLoaderView()
{
    loaderWidget = new QWidget();
    QVBoxLayout *mainLayout = new QVBoxLayout(loaderWidget);
    mainLayout->setSpacing(30);
    mainLayout->setContentsMargins(50, 50, 50, 50);
    mainLayout->setAlignment(Qt::AlignCenter);

    // Animated brain icon
    QLabel *brainIcon = new QLabel("🧠");
    brainIcon->setObjectName("brainIcon");
    brainIcon->setStyleSheet("font-size: 80px; color: #4a90e2;");
    brainIcon->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(brainIcon);

    // Loading text
    QLabel *loadingText = new QLabel("Анализируем модель...");
    loadingText->setObjectName("loadingText");
    loadingText->setStyleSheet("font-size: 24px; font-weight: bold; color: #495057;");
    loadingText->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(loadingText);

    // Progress bar
    QProgressBar *loaderProgressBar = new QProgressBar();
    loaderProgressBar->setObjectName("loaderProgressBar");
    loaderProgressBar->setRange(0, 0); // Indeterminate progress
    loaderProgressBar->setMinimumHeight(8);
    loaderProgressBar->setMaximumHeight(8);
    loaderProgressBar->setStyleSheet(R"(
        QProgressBar {
            border: none;
            border-radius: 4px;
            background-color: #e9ecef;
        }
        QProgressBar::chunk {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #4a90e2, stop:0.5 #6bb6ff, stop:1 #4a90e2);
            border-radius: 4px;
        }
    )");
    mainLayout->addWidget(loaderProgressBar);

    // Status text
    QLabel *statusText = new QLabel("Пожалуйста, подождите...");
    statusText->setObjectName("statusText");
    statusText->setStyleSheet("font-size: 16px; color: #6c757d; margin-top: 20px;");
    statusText->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(statusText);

    // Add some spacing
    mainLayout->addStretch(1);

    // Add to stacked widget
    stackedWidget->addWidget(loaderWidget);
}

void MainWindow::createAnalysisView()
{
    analysisWidget = new QWidget();
    QVBoxLayout *mainLayout = new QVBoxLayout(analysisWidget);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // Header with back button and title
    QHBoxLayout *headerLayout = new QHBoxLayout();
    
    backButton = new QPushButton("← Назад");
    backButton->setObjectName("backButton");
    backButton->setMaximumWidth(100);
    backButton->setMinimumHeight(35);
    headerLayout->addWidget(backButton);
    
    headerLayout->addStretch();
    
    modelTitleLabel = new QLabel("Анализ модели");
    modelTitleLabel->setStyleSheet("font-size: 20px; font-weight: bold; color: #212529;");
    headerLayout->addWidget(modelTitleLabel);
    
    headerLayout->addStretch();
    
    mainLayout->addLayout(headerLayout);

    // Model path
    modelPathLabel = new QLabel();
    modelPathLabel->setStyleSheet("font-size: 12px; color: #6c757d; margin-bottom: 15px; padding: 8px; background-color: #e9ecef; border-radius: 6px;");
    modelPathLabel->setWordWrap(true);
    modelPathLabel->setMinimumHeight(40);
    mainLayout->addWidget(modelPathLabel);

    // Progress bar
    progressBar = new QProgressBar();
    progressBar->setVisible(false);
    progressBar->setRange(0, 0); // Indeterminate progress
    progressBar->setMinimumHeight(25);
    mainLayout->addWidget(progressBar);

    // Model tree
    modelTree = new QTreeWidget();
    modelTree->setHeaderLabels(QStringList() << "Слой/Информация" << "Тип/Значение" << "Параметры" << "Нейроны" << "Форма входа" << "Форма выхода/Размер");
    modelTree->setAlternatingRowColors(true);
    modelTree->setMinimumHeight(300);
    mainLayout->addWidget(modelTree);

    // Buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    
    analyzeButton = new QPushButton("🔄 Анализировать модель");
    analyzeButton->setStyleSheet("font-size: 14px; padding: 12px 25px;");
    analyzeButton->setMinimumHeight(40);
    buttonLayout->addWidget(analyzeButton);
    
    buttonLayout->addStretch();
    
    simplifyButton = new QPushButton("⚡ Упростить модель");
    simplifyButton->setObjectName("simplifyButton");
    simplifyButton->setStyleSheet("font-size: 14px; padding: 12px 25px;");
    simplifyButton->setMinimumHeight(40);
    simplifyButton->setEnabled(false); // Initially disabled
    buttonLayout->addWidget(simplifyButton);
    
    mainLayout->addLayout(buttonLayout);

    // Log output
    QLabel *logLabel = new QLabel("Лог выполнения:");
    logLabel->setStyleSheet("font-weight: bold; margin-top: 15px;");
    mainLayout->addWidget(logLabel);
    
    logOutput = new QTextEdit();
    logOutput->setMaximumHeight(120);
    logOutput->setMinimumHeight(80);
    logOutput->setReadOnly(true);
    mainLayout->addWidget(logOutput);

    // Connect signals
    connect(backButton, &QPushButton::clicked, this, &MainWindow::goBackToFileSelection);
    connect(analyzeButton, &QPushButton::clicked, this, &MainWindow::analyzeModel);
    connect(simplifyButton, &QPushButton::clicked, this, &MainWindow::simplifyModel);
    
    // Add to stacked widget
    stackedWidget->addWidget(analysisWidget);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls() && stackedWidget->currentWidget() == fileSelectionWidget) {
        event->acceptProposedAction();
        dropArea->setStyleSheet(R"(
            QFrame#dropArea {
                border: 3px dashed #007bff;
                border-radius: 12px;
                background-color: #f8f9ff;
                margin: 20px;
            }
        )");
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    if (stackedWidget->currentWidget() != fileSelectionWidget) {
        return;
    }
    
    const QMimeData *mimeData = event->mimeData();
    
    if (mimeData->hasUrls()) {
        QList<QUrl> urlList = mimeData->urls();
        if (!urlList.isEmpty()) {
            QString filePath = urlList.first().toLocalFile();
            loadFile(filePath);
        }
    }
    
    // Reset drop area style
    dropArea->setStyleSheet(R"(
        QFrame#dropArea {
            border: 3px dashed #dee2e6;
            border-radius: 12px;
            background-color: #ffffff;
            margin: 20px;
        }
        QFrame#dropArea:hover {
            border-color: #007bff;
            background-color: #f8f9ff;
        }
    )");
    
    event->acceptProposedAction();
}

void MainWindow::selectFile()
{
    QString fileName = QFileDialog::getOpenFileName(
        this, 
        "Выберите модель", 
        "", 
        "Модели (*.h5 *.keras *.model *.pkl *.pth *.pt);;Keras модели (*.h5 *.keras *.model);;PyTorch модели (*.pkl *.pth *.pt);;Все файлы (*.*)"
    );
    
    if (!fileName.isEmpty()) {
        loadFile(fileName);
    }
}

void MainWindow::loadFile(const QString &filePath)
{
    QFileInfo fileInfo(filePath);
    
    if (!fileInfo.exists()) {
        QMessageBox::warning(this, "Ошибка", "Файл не найден: " + filePath);
        return;
    }
    
    currentFilePath = filePath;
    updateFileInfo(filePath);
    
    // Show analyze button
    analyzeFileButton->setVisible(true);
}

void MainWindow::updateFileInfo(const QString &filePath)
{
    QFileInfo fileInfo(filePath);
    
    QString fileInfoText = QString("📄 <b>%1</b><br>"
                                  "📁 Путь: %2<br>"
                                  "📏 Размер: %3 байт<br>"
                                  "📅 Изменен: %4")
                                  .arg(fileInfo.fileName())
                                  .arg(filePath)
                                  .arg(fileInfo.size())
                                  .arg(fileInfo.lastModified().toString("dd.MM.yyyy hh:mm:ss"));
    
    filePathLabel->setText(fileInfoText);
    filePathLabel->setStyleSheet("font-size: 14px; color: #495057; margin-top: 20px; padding: 15px; background-color: #d4edda; border: 1px solid #c3e6cb; border-radius: 8px;");
}

void MainWindow::startAnalysis()
{
    // Update analysis view with file info
    QFileInfo fileInfo(currentFilePath);
    modelTitleLabel->setText("Анализ модели: " + fileInfo.fileName());
    modelPathLabel->setText("📁 " + currentFilePath);
    
    // Clear previous data
    modelTree->clear();
    logOutput->clear();
    
    // Disable simplify button until analysis is complete
    simplifyButton->setEnabled(false);
    
    // Switch to loader view
    if (loaderWidget) {
        stackedWidget->setCurrentWidget(loaderWidget);
        
        // Start loader animation
        startLoaderAnimation();
    } else {
        qDebug() << "Loader widget is null, skipping to analysis";
        analyzeModel();
        return;
    }
    
    // Start analysis after a short delay
    QTimer::singleShot(2000, this, &MainWindow::analyzeModel);
}

void MainWindow::switchToAnalysisView()
{
    // This function is now replaced by startAnalysis()
    startAnalysis();
}

void MainWindow::analyzeModel()
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
    
    logOutput->append("🔍 Начинаем анализ модели...");
    logOutput->append("📁 Путь к модели: " + currentFilePath);
    progressBar->setVisible(true);
    
    // Disable simplify button during analysis
    simplifyButton->setEnabled(false);
    
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

void MainWindow::handlePythonOutput()
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
            populateModelTree(modelData);
            logOutput->append("✅ Анализ завершен успешно!");
            
            // Enable simplify button after successful analysis
            simplifyButton->setEnabled(true);
            
            // Switch from loader to analysis view
            stackedWidget->setCurrentWidget(analysisWidget);
        }
    }
}

void MainWindow::handlePythonError()
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


void MainWindow::populateModelTree(const QJsonObject &modelData)
{
    modelTree->clear();
    
    // BASIC INFORMATION section
    QTreeWidgetItem *basicHeader = new QTreeWidgetItem(modelTree);
    basicHeader->setText(0, "📋 ОСНОВНАЯ ИНФОРМАЦИЯ");
    basicHeader->setText(1, "");
    basicHeader->setText(2, "");
    basicHeader->setText(3, "");
    basicHeader->setText(4, "");
    basicHeader->setText(5, "");
    basicHeader->setBackground(0, QColor(74, 144, 226));
    basicHeader->setBackground(1, QColor(74, 144, 226));
    basicHeader->setBackground(2, QColor(74, 144, 226));
    basicHeader->setBackground(3, QColor(74, 144, 226));
    basicHeader->setBackground(4, QColor(74, 144, 226));
    basicHeader->setBackground(5, QColor(74, 144, 226));
    basicHeader->setForeground(0, QColor(255, 255, 255));
    basicHeader->setForeground(1, QColor(255, 255, 255));
    basicHeader->setForeground(2, QColor(255, 255, 255));
    basicHeader->setForeground(3, QColor(255, 255, 255));
    basicHeader->setForeground(4, QColor(255, 255, 255));
    basicHeader->setForeground(5, QColor(255, 255, 255));
    
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
    
    // Optimizer information
    if (modelData.contains("optimizer_info") && !modelData["optimizer_info"].toObject().isEmpty()) {
        QJsonObject optimizerInfo = modelData["optimizer_info"].toObject();
        QTreeWidgetItem *optimizerItem = new QTreeWidgetItem(modelTree);
        optimizerItem->setText(0, "Оптимизатор");
        optimizerItem->setText(1, optimizerInfo["name"].toString());
        optimizerItem->setText(2, "");
        optimizerItem->setText(3, "");
        optimizerItem->setText(4, "");
        optimizerItem->setText(5, "");
        
        if (optimizerInfo.contains("learning_rate") && optimizerInfo["learning_rate"].toString() != "N/A") {
            QTreeWidgetItem *lrItem = new QTreeWidgetItem(modelTree);
            lrItem->setText(0, "Скорость обучения");
            lrItem->setText(1, QString::number(optimizerInfo["learning_rate"].toDouble(), 'g', 6));
            lrItem->setText(2, "");
            lrItem->setText(3, "");
            lrItem->setText(4, "");
            lrItem->setText(5, "");
        }
    }
    
    // Loss function information
    if (modelData.contains("loss_info") && !modelData["loss_info"].toObject().isEmpty()) {
        QJsonObject lossInfo = modelData["loss_info"].toObject();
        QTreeWidgetItem *lossItem = new QTreeWidgetItem(modelTree);
        lossItem->setText(0, "Функция потерь");
        lossItem->setText(1, lossInfo["name"].toString());
        lossItem->setText(2, "");
        lossItem->setText(3, "");
        lossItem->setText(4, "");
        lossItem->setText(5, "");
    }
    
    // MODEL STRUCTURE section
    QTreeWidgetItem *structureHeader = new QTreeWidgetItem(modelTree);
    structureHeader->setText(0, "🏗️ СТРУКТУРА МОДЕЛИ");
    structureHeader->setText(1, "");
    structureHeader->setText(2, "");
    structureHeader->setText(3, "");
    structureHeader->setText(4, "");
    structureHeader->setText(5, "");
    structureHeader->setBackground(0, QColor(74, 144, 226));
    structureHeader->setBackground(1, QColor(74, 144, 226));
    structureHeader->setBackground(2, QColor(74, 144, 226));
    structureHeader->setBackground(3, QColor(74, 144, 226));
    structureHeader->setBackground(4, QColor(74, 144, 226));
    structureHeader->setBackground(5, QColor(74, 144, 226));
    structureHeader->setForeground(0, QColor(255, 255, 255));
    structureHeader->setForeground(1, QColor(255, 255, 255));
    structureHeader->setForeground(2, QColor(255, 255, 255));
    structureHeader->setForeground(3, QColor(255, 255, 255));
    structureHeader->setForeground(4, QColor(255, 255, 255));
    structureHeader->setForeground(5, QColor(255, 255, 255));
    
    // Add detailed layer information (only for Keras models)
    if (modelData.contains("layers")) {
        QJsonArray layers = modelData["layers"].toArray();
        
        // Total layers count
        QTreeWidgetItem *layersCountItem = new QTreeWidgetItem(modelTree);
        layersCountItem->setText(0, "Всего слоев");
        layersCountItem->setText(1, QString::number(layers.size()));
        layersCountItem->setText(2, "");
        layersCountItem->setText(3, "");
        layersCountItem->setText(4, "");
        layersCountItem->setText(5, "");

        for (const QJsonValue &layerVal : layers) {
            QJsonObject layer = layerVal.toObject();

            QTreeWidgetItem *item = new QTreeWidgetItem(modelTree);
            item->setText(0, layer["name"].toString());
            
            // Показываем тип и активацию
            QString typeAndActivation = layer["type"].toString();
            if (layer.contains("activation") && layer["activation"].toString() != "N/A") {
                typeAndActivation += " (" + layer["activation"].toString() + ")";
            }
            item->setText(1, typeAndActivation);
            
            item->setText(2, QString::number(layer["params"].toInt()));
            item->setText(3, QString::number(layer["neurons"].toInt()));
            item->setText(4, layer["input_shape"].toString());
            item->setText(5, layer["output_shape"].toString());
            
            // Добавляем информацию о весах, если есть
            if (layer.contains("weights_info") && !layer["weights_info"].toObject().isEmpty()) {
                QJsonObject weightsInfo = layer["weights_info"].toObject();
                
                if (weightsInfo.contains("weights_shape")) {
                    QTreeWidgetItem *weightsItem = new QTreeWidgetItem(item);
                    weightsItem->setText(0, "  └─ Веса");
                    weightsItem->setText(1, weightsInfo["weights_shape"].toString());
                    weightsItem->setText(2, "");
                    weightsItem->setText(3, "");
                    weightsItem->setText(4, weightsInfo.contains("weights_range") ? weightsInfo["weights_range"].toString() : "");
                    weightsItem->setText(5, "");
                }
                
                if (weightsInfo.contains("biases_shape")) {
                    QTreeWidgetItem *biasesItem = new QTreeWidgetItem(item);
                    biasesItem->setText(0, "  └─ Смещения");
                    biasesItem->setText(1, weightsInfo["biases_shape"].toString());
                    biasesItem->setText(2, "");
                    biasesItem->setText(3, "");
                    biasesItem->setText(4, weightsInfo.contains("biases_range") ? weightsInfo["biases_range"].toString() : "");
                    biasesItem->setText(5, "");
                }
            }
        }
    } else {
        // For PyTorch models or models without layer info, show basic info
        QTreeWidgetItem *infoItem = new QTreeWidgetItem(modelTree);
        infoItem->setText(0, "ℹ️ Информация о модели");
        infoItem->setText(1, modelData.contains("framework") ? modelData["framework"].toString() : "Unknown");
        infoItem->setText(2, QString::number(modelData["total_params"].toInt()));
        infoItem->setText(3, "N/A");
        infoItem->setText(4, "N/A");
        infoItem->setText(5, "N/A");
    }
    
    // Expand the tree
    modelTree->expandAll();
}

void MainWindow::simplifyModel()
{
    logOutput->append("⚡ Функция упрощения модели пока в разработке...");
    QMessageBox::information(this, "Информация", "Функция упрощения модели будет добавлена в следующих версиях.");
}

void MainWindow::goBackToFileSelection()
{
    stackedWidget->setCurrentWidget(fileSelectionWidget);
}

void MainWindow::showDependencyError()
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

bool MainWindow::checkPythonEnvironment()
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

void MainWindow::animateTransition(QWidget *from, QWidget *to)
{
    if (!from || !to) return;
    
    // Create opacity effects with proper parent
    QGraphicsOpacityEffect *fromEffect = new QGraphicsOpacityEffect(from);
    QGraphicsOpacityEffect *toEffect = new QGraphicsOpacityEffect(to);
    
    from->setGraphicsEffect(fromEffect);
    to->setGraphicsEffect(toEffect);
    
    // Set initial states
    fromEffect->setOpacity(1.0);
    toEffect->setOpacity(0.0);
    
    // Create animations with proper parent
    QPropertyAnimation *fadeOut = new QPropertyAnimation(fromEffect, "opacity", from);
    fadeOut->setDuration(300);
    fadeOut->setStartValue(1.0);
    fadeOut->setEndValue(0.0);
    
    QPropertyAnimation *fadeIn = new QPropertyAnimation(toEffect, "opacity", to);
    fadeIn->setDuration(300);
    fadeIn->setStartValue(0.0);
    fadeIn->setEndValue(1.0);
    
    // Create sequential animation group with proper parent
    QSequentialAnimationGroup *animationGroup = new QSequentialAnimationGroup(this);
    animationGroup->addAnimation(fadeOut);
    animationGroup->addAnimation(fadeIn);
    
    // Switch widget in the middle of animation
    connect(fadeOut, &QPropertyAnimation::finished, [this, to]() {
        if (stackedWidget && to) {
            stackedWidget->setCurrentWidget(to);
        }
    });
    
    // Clean up effects when animation is done
    connect(animationGroup, &QSequentialAnimationGroup::finished, [fromEffect, toEffect]() {
        if (fromEffect) fromEffect->deleteLater();
        if (toEffect) toEffect->deleteLater();
    });
    
    // Start animation
    animationGroup->start(QAbstractAnimation::DeleteWhenStopped);
}

void MainWindow::startLoaderAnimation()
{
    if (!loaderWidget) {
        qDebug() << "Loader widget is null";
        return;
    }
    
    // Find the brain icon and loading text
    QLabel *brainIcon = loaderWidget->findChild<QLabel*>("brainIcon");
    QLabel *loadingText = loaderWidget->findChild<QLabel*>("loadingText");
    QLabel *statusText = loaderWidget->findChild<QLabel*>("statusText");
    
    if (!brainIcon || !loadingText || !statusText) {
        qDebug() << "Could not find loader elements";
        return;
    }
    
    // Create status text animation
    static QStringList statusMessages = {
        "Загружаем модель...",
        "Анализируем архитектуру...",
        "Подсчитываем параметры...",
        "Обрабатываем слои...",
        "Готовим результаты..."
    };
    
    QTimer *statusTimer = new QTimer(loaderWidget);
    static int statusIndex = 0;
    connect(statusTimer, &QTimer::timeout, [statusText]() {
        if (statusText) {
            statusText->setText(statusMessages[statusIndex % statusMessages.size()]);
            statusIndex++;
        }
    });
    statusTimer->start(400); // Change status every 400ms
}


