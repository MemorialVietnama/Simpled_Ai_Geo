#include "mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , stackedWidget(nullptr)
    , sceneManager(nullptr)
{
    setupUI();
    applyStyles();
}

MainWindow::~MainWindow()
{
    // SceneManager will handle cleanup of scenes
}

void MainWindow::setupUI()
{
    // Set window properties
    setWindowTitle("OptimizerGPT - AI Model Optimizer");
    setMinimumSize(800, 600);
    setAcceptDrops(true);
    
    // Create stacked widget
    stackedWidget = new QStackedWidget(this);
    setCentralWidget(stackedWidget);
    
    // Create scene manager
    sceneManager = new SceneManager(stackedWidget, this);
    
    // Connect scene manager signals
    connect(sceneManager, &SceneManager::sceneChanged, 
            this, &MainWindow::onSceneChanged);
}

void MainWindow::applyStyles()
{
    // Apply modern styling
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
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // Stop any running analysis
    if (sceneManager && sceneManager->getAnalysisScene()) {
        sceneManager->getAnalysisScene()->stopAnalysis();
    }
    
    // Accept the close event
    event->accept();
}

void MainWindow::onSceneChanged(const QString &sceneName)
{
    // Optional: Handle scene change events
    // For example, update window title or status bar
    qDebug() << "Scene changed to:" << sceneName;
}
