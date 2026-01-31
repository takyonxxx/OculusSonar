#pragma once

#include <QWidget>
#include <QSlider>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>

struct DetectionParameters {
    double highThresholdMult = 1.65;
    double lowThresholdMult = 1.65;
    int minBlobArea = 450;
    int maxBlobArea = 25000;
    int minWidth = 25;
    int maxWidth = 400;
    int minHeight = 25;
    int maxHeight = 400;
    double minIntensityDiff = 20.0;
    double minAspectRatio = 0.3;
    double maxAspectRatio = 3.0;
    double minSolidity = 0.3;
    double minCompactness = 0.2;
};

class DetectionParamsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DetectionParamsWidget(QWidget *parent = nullptr);
    
    DetectionParameters getParams() const { return m_params; }

signals:
    void paramsChanged(const DetectionParameters& params);

private slots:
    void onSliderChanged();

private:
    QSlider* createSlider(int min, int max, int value, const QString& label, QLabel** valueLabel);
    QSlider* createDoubleSlider(double min, double max, double value, const QString& label, QLabel** valueLabel, int scale = 100);

    DetectionParameters m_params;
    
    QSlider* m_highThresholdSlider;
    QSlider* m_lowThresholdSlider;
    QSlider* m_minBlobAreaSlider;
    QSlider* m_maxBlobAreaSlider;
    QSlider* m_minWidthSlider;
    QSlider* m_maxWidthSlider;
    QSlider* m_minHeightSlider;
    QSlider* m_maxHeightSlider;
    QSlider* m_minIntensityDiffSlider;
    QSlider* m_minAspectRatioSlider;
    QSlider* m_maxAspectRatioSlider;
    QSlider* m_minSoliditySlider;
    QSlider* m_minCompactnessSlider;
    
    QLabel* m_highThresholdLabel;
    QLabel* m_lowThresholdLabel;
    QLabel* m_minBlobAreaLabel;
    QLabel* m_maxBlobAreaLabel;
    QLabel* m_minWidthLabel;
    QLabel* m_maxWidthLabel;
    QLabel* m_minHeightLabel;
    QLabel* m_maxHeightLabel;
    QLabel* m_minIntensityDiffLabel;
    QLabel* m_minAspectRatioLabel;
    QLabel* m_maxAspectRatioLabel;
    QLabel* m_minSolidityLabel;
    QLabel* m_minCompactnessLabel;
};
