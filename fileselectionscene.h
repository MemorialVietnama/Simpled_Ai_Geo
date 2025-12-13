#ifndef FILESELECTIONSCENE_H
#define FILESELECTIONSCENE_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QUrl>
#include <QMimeData>
#include <QFileInfo>
#include <QMessageBox>

class FileSelectionScene : public QWidget
{
    Q_OBJECT

public:
    explicit FileSelectionScene(QWidget *parent = nullptr);
    ~FileSelectionScene();

    QString getSelectedFilePath() const;
    QString getTrainingDataPath() const { return trainingDataPath; }
    void clearSelection();

signals:
    void fileSelected(const QString &filePath);
    void analysisRequested();

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void selectFile();
    void selectTrainingData();
    void startAnalysis();

private:
    void setupUI();
    void loadFile(const QString &filePath);
    void updateFileInfo(const QString &filePath);
    void updateTrainFileInfo(const QString &filePath);
    void updateDropAreaStyle(bool isDragOver);
    void updateAnalyzeAvailability();

    // UI Elements
    QPushButton *selectFileButton;
    QPushButton *selectTrainDataButton;
    QPushButton *analyzeFileButton;
    QLabel *filePathLabel;
    QLabel *trainPathLabel;
    QLabel *dropAreaLabel;
    QFrame *dropArea;
    
    // Data
    QString currentFilePath;
    QString trainingDataPath;
};

#endif // FILESELECTIONSCENE_H
