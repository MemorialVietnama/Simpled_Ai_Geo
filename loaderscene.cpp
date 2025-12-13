#include "loaderscene.h"

LoaderScene::LoaderScene(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
}

LoaderScene::~LoaderScene()
{
}

void LoaderScene::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(30);
    mainLayout->setContentsMargins(50, 50, 50, 50);
    mainLayout->setAlignment(Qt::AlignCenter);

    // Animated brain icon
    brainIcon = new QLabel("🧠");
    brainIcon->setObjectName("brainIcon");
    brainIcon->setStyleSheet("font-size: 80px; color: #4a90e2;");
    brainIcon->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(brainIcon);

    // Loading text
    loadingText = new QLabel("Анализируем модель...");
    loadingText->setObjectName("loadingText");
    loadingText->setStyleSheet("font-size: 24px; font-weight: bold; color: #495057;");
    loadingText->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(loadingText);

    // Progress bar
    progressBar = new QProgressBar();
    progressBar->setObjectName("loaderProgressBar");
    progressBar->setRange(0, 0); // Indeterminate progress
    progressBar->setMinimumHeight(8);
    progressBar->setMaximumHeight(8);
    progressBar->setStyleSheet(R"(
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
    mainLayout->addWidget(progressBar);

    // Status text
    statusText = new QLabel("Пожалуйста, подождите...");
    statusText->setObjectName("statusText");
    statusText->setStyleSheet("font-size: 16px; color: #6c757d; margin-top: 20px;");
    statusText->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    statusText->setWordWrap(true);
    mainLayout->addWidget(statusText);

    // Add some spacing
    mainLayout->addStretch(1);
}

void LoaderScene::startAnimation()
{
    progressBar->setRange(0, 0); // Indeterminate
    resetStatus();
    appendStatusMessage("🔁 Подготовка к анализу...");
}

void LoaderScene::stopAnimation()
{
    progressBar->setRange(0, 1);
    progressBar->setValue(1);
}

void LoaderScene::resetStatus()
{
    statusLog.clear();
    statusText->setText("Ожидание данных...");
}

void LoaderScene::appendStatusMessage(const QString &message)
{
    if (message.trimmed().isEmpty()) {
        return;
    }
    statusLog.append(message.trimmed());
    while (statusLog.size() > maxLogLines) {
        statusLog.removeFirst();
    }
    updateStatusLabel();
}

void LoaderScene::handleLogReset()
{
    resetStatus();
}

void LoaderScene::handleLogMessage(const QString &message)
{
    appendStatusMessage(message);
}

void LoaderScene::updateStatusLabel()
{
    if (!statusText) {
        return;
    }
    statusText->setText(statusLog.join("\n"));
}
