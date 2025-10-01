#include "fileselectionscene.h"
#include <QApplication>

FileSelectionScene::FileSelectionScene(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
}

FileSelectionScene::~FileSelectionScene()
{
}

void FileSelectionScene::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(20);
    mainLayout->setContentsMargins(30, 30, 30, 30);

    // Create title
    QLabel *titleLabel = new QLabel("OptimizerGPT");
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
    dropArea->setAcceptDrops(true);
    
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
    connect(selectFileButton, &QPushButton::clicked, this, &FileSelectionScene::selectFile);
    connect(analyzeFileButton, &QPushButton::clicked, this, &FileSelectionScene::startAnalysis);
}

void FileSelectionScene::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
        updateDropAreaStyle(true);
    }
}

void FileSelectionScene::dropEvent(QDropEvent *event)
{
    const QMimeData *mimeData = event->mimeData();
    
    if (mimeData->hasUrls()) {
        QList<QUrl> urlList = mimeData->urls();
        if (!urlList.isEmpty()) {
            QString filePath = urlList.first().toLocalFile();
            loadFile(filePath);
        }
    }
    
    updateDropAreaStyle(false);
    event->acceptProposedAction();
}

void FileSelectionScene::selectFile()
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

void FileSelectionScene::startAnalysis()
{
    if (!currentFilePath.isEmpty()) {
        emit analysisRequested();
    }
}

void FileSelectionScene::loadFile(const QString &filePath)
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
    
    emit fileSelected(filePath);
}

void FileSelectionScene::updateFileInfo(const QString &filePath)
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

void FileSelectionScene::updateDropAreaStyle(bool isDragOver)
{
    if (isDragOver) {
        dropArea->setStyleSheet(R"(
            QFrame#dropArea {
                border: 3px dashed #007bff;
                border-radius: 12px;
                background-color: #f8f9ff;
                margin: 20px;
            }
        )");
    } else {
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
    }
}

QString FileSelectionScene::getSelectedFilePath() const
{
    return currentFilePath;
}

void FileSelectionScene::clearSelection()
{
    currentFilePath.clear();
    filePathLabel->setText("Модель не выбрана");
    filePathLabel->setStyleSheet("font-size: 12px; color: #6c757d; margin-top: 15px; padding: 10px; background-color: #e9ecef; border-radius: 6px;");
    analyzeFileButton->setVisible(false);
    updateDropAreaStyle(false);
}
