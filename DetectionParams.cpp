#include "DetectionParams.h"

DetectionParamsWidget::DetectionParamsWidget(QWidget *parent)
    : QWidget(parent)
{
    // Dark theme stylesheet
    setStyleSheet(
        "QWidget {"
        "   background-color: #2b2b2b;"
        "   color: #ffffff;"
        "}"
        "QGroupBox {"
        "   border: 2px solid #00FFFF;"
        "   border-radius: 5px;"
        "   margin-top: 10px;"
        "   padding: 10px;"
        "   font-weight: bold;"
        "   color: #00FFFF;"
        "}"
        "QGroupBox::title {"
        "   subcontrol-origin: margin;"
        "   left: 10px;"
        "   padding: 0 5px;"
        "   color: #00FFFF;"
        "}"
        "QLabel {"
        "   color: #ffffff;"
        "   font-size: 11px;"
        "}"
        "QSlider::groove:horizontal {"
        "   border: 1px solid #555;"
        "   height: 8px;"
        "   background: #444;"
        "   border-radius: 4px;"
        "}"
        "QSlider::handle:horizontal {"
        "   background: #00FFFF;"
        "   border: 1px solid #00CCCC;"
        "   width: 18px;"
        "   margin: -5px 0;"
        "   border-radius: 9px;"
        "}"
        "QSlider::handle:horizontal:hover {"
        "   background: #00DDDD;"
        "}"
    );
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // Threshold Group
    QGroupBox* thresholdGroup = new QGroupBox("Threshold Parametreleri");
    QVBoxLayout* thresholdLayout = new QVBoxLayout();
    
    m_highThresholdSlider = createDoubleSlider(0.5, 4.0, 1.65, "Parlak Eşik Çarpanı", &m_highThresholdLabel);
    thresholdLayout->addWidget(m_highThresholdSlider);
    
    m_lowThresholdSlider = createDoubleSlider(0.5, 4.0, 1.65, "Koyu Eşik Çarpanı", &m_lowThresholdLabel);
    thresholdLayout->addWidget(m_lowThresholdSlider);
    
    thresholdGroup->setLayout(thresholdLayout);
    mainLayout->addWidget(thresholdGroup);
    
    // Size Group
    QGroupBox* sizeGroup = new QGroupBox("Boyut Parametreleri");
    QVBoxLayout* sizeLayout = new QVBoxLayout();
    
    m_minBlobAreaSlider = createSlider(100, 2000, 450, "Min Alan", &m_minBlobAreaLabel);
    sizeLayout->addWidget(m_minBlobAreaSlider);
    
    m_maxBlobAreaSlider = createSlider(5000, 50000, 25000, "Max Alan", &m_maxBlobAreaLabel);
    sizeLayout->addWidget(m_maxBlobAreaSlider);
    
    m_minWidthSlider = createSlider(5, 100, 25, "Min Genişlik", &m_minWidthLabel);
    sizeLayout->addWidget(m_minWidthSlider);
    
    m_maxWidthSlider = createSlider(100, 400, 400, "Max Genişlik", &m_maxWidthLabel);
    sizeLayout->addWidget(m_maxWidthSlider);
    
    m_minHeightSlider = createSlider(5, 100, 25, "Min Yükseklik", &m_minHeightLabel);
    sizeLayout->addWidget(m_minHeightSlider);
    
    m_maxHeightSlider = createSlider(100, 400, 400, "Max Yükseklik", &m_maxHeightLabel);
    sizeLayout->addWidget(m_maxHeightSlider);
    
    sizeGroup->setLayout(sizeLayout);
    mainLayout->addWidget(sizeGroup);
    
    // Quality Group
    QGroupBox* qualityGroup = new QGroupBox("Kalite Parametreleri");
    QVBoxLayout* qualityLayout = new QVBoxLayout();
    
    m_minIntensityDiffSlider = createDoubleSlider(5.0, 50.0, 20.0, "Min Parlaklık Farkı", &m_minIntensityDiffLabel);
    qualityLayout->addWidget(m_minIntensityDiffSlider);
    
    m_minAspectRatioSlider = createDoubleSlider(0.1, 1.0, 0.3, "Min En/Boy Oranı", &m_minAspectRatioLabel);
    qualityLayout->addWidget(m_minAspectRatioSlider);
    
    m_maxAspectRatioSlider = createDoubleSlider(1.0, 5.0, 3.0, "Max En/Boy Oranı", &m_maxAspectRatioLabel);
    qualityLayout->addWidget(m_maxAspectRatioSlider);
    
    m_minSoliditySlider = createDoubleSlider(0.1, 1.0, 0.3, "Min Doluluk", &m_minSolidityLabel);
    qualityLayout->addWidget(m_minSoliditySlider);
    
    m_minCompactnessSlider = createDoubleSlider(0.05, 0.5, 0.2, "Min Kompaktlık", &m_minCompactnessLabel);
    qualityLayout->addWidget(m_minCompactnessSlider);
    
    qualityGroup->setLayout(qualityLayout);
    mainLayout->addWidget(qualityGroup);
    
    mainLayout->addStretch();
    
    // Connect all sliders
    connect(m_highThresholdSlider, &QSlider::valueChanged, this, &DetectionParamsWidget::onSliderChanged);
    connect(m_lowThresholdSlider, &QSlider::valueChanged, this, &DetectionParamsWidget::onSliderChanged);
    connect(m_minBlobAreaSlider, &QSlider::valueChanged, this, &DetectionParamsWidget::onSliderChanged);
    connect(m_maxBlobAreaSlider, &QSlider::valueChanged, this, &DetectionParamsWidget::onSliderChanged);
    connect(m_minWidthSlider, &QSlider::valueChanged, this, &DetectionParamsWidget::onSliderChanged);
    connect(m_maxWidthSlider, &QSlider::valueChanged, this, &DetectionParamsWidget::onSliderChanged);
    connect(m_minHeightSlider, &QSlider::valueChanged, this, &DetectionParamsWidget::onSliderChanged);
    connect(m_maxHeightSlider, &QSlider::valueChanged, this, &DetectionParamsWidget::onSliderChanged);
    connect(m_minIntensityDiffSlider, &QSlider::valueChanged, this, &DetectionParamsWidget::onSliderChanged);
    connect(m_minAspectRatioSlider, &QSlider::valueChanged, this, &DetectionParamsWidget::onSliderChanged);
    connect(m_maxAspectRatioSlider, &QSlider::valueChanged, this, &DetectionParamsWidget::onSliderChanged);
    connect(m_minSoliditySlider, &QSlider::valueChanged, this, &DetectionParamsWidget::onSliderChanged);
    connect(m_minCompactnessSlider, &QSlider::valueChanged, this, &DetectionParamsWidget::onSliderChanged);
}

QSlider* DetectionParamsWidget::createSlider(int min, int max, int value, const QString& label, QLabel** valueLabel)
{
    QHBoxLayout* layout = new QHBoxLayout();
    
    QLabel* nameLabel = new QLabel(label + ":");
    nameLabel->setMinimumWidth(150);
    layout->addWidget(nameLabel);
    
    QSlider* slider = new QSlider(Qt::Horizontal);
    slider->setMinimum(min);
    slider->setMaximum(max);
    slider->setValue(value);
    layout->addWidget(slider);
    
    *valueLabel = new QLabel(QString::number(value));
    (*valueLabel)->setMinimumWidth(60);
    layout->addWidget(*valueLabel);
    
    QWidget* container = new QWidget();
    container->setLayout(layout);
    this->layout()->addWidget(container);
    
    return slider;
}

QSlider* DetectionParamsWidget::createDoubleSlider(double min, double max, double value, const QString& label, QLabel** valueLabel, int scale)
{
    QHBoxLayout* layout = new QHBoxLayout();
    
    QLabel* nameLabel = new QLabel(label + ":");
    nameLabel->setMinimumWidth(150);
    layout->addWidget(nameLabel);
    
    QSlider* slider = new QSlider(Qt::Horizontal);
    slider->setMinimum((int)(min * scale));
    slider->setMaximum((int)(max * scale));
    slider->setValue((int)(value * scale));
    layout->addWidget(slider);
    
    *valueLabel = new QLabel(QString::number(value, 'f', 2));
    (*valueLabel)->setMinimumWidth(60);
    layout->addWidget(*valueLabel);
    
    QWidget* container = new QWidget();
    container->setLayout(layout);
    this->layout()->addWidget(container);
    
    return slider;
}

void DetectionParamsWidget::onSliderChanged()
{
    m_params.highThresholdMult = m_highThresholdSlider->value() / 100.0;
    m_params.lowThresholdMult = m_lowThresholdSlider->value() / 100.0;
    m_params.minBlobArea = m_minBlobAreaSlider->value();
    m_params.maxBlobArea = m_maxBlobAreaSlider->value();
    m_params.minWidth = m_minWidthSlider->value();
    m_params.maxWidth = m_maxWidthSlider->value();
    m_params.minHeight = m_minHeightSlider->value();
    m_params.maxHeight = m_maxHeightSlider->value();
    m_params.minIntensityDiff = m_minIntensityDiffSlider->value() / 100.0;
    m_params.minAspectRatio = m_minAspectRatioSlider->value() / 100.0;
    m_params.maxAspectRatio = m_maxAspectRatioSlider->value() / 100.0;
    m_params.minSolidity = m_minSoliditySlider->value() / 100.0;
    m_params.minCompactness = m_minCompactnessSlider->value() / 100.0;
    
    m_highThresholdLabel->setText(QString::number(m_params.highThresholdMult, 'f', 2));
    m_lowThresholdLabel->setText(QString::number(m_params.lowThresholdMult, 'f', 2));
    m_minBlobAreaLabel->setText(QString::number(m_params.minBlobArea));
    m_maxBlobAreaLabel->setText(QString::number(m_params.maxBlobArea));
    m_minWidthLabel->setText(QString::number(m_params.minWidth));
    m_maxWidthLabel->setText(QString::number(m_params.maxWidth));
    m_minHeightLabel->setText(QString::number(m_params.minHeight));
    m_maxHeightLabel->setText(QString::number(m_params.maxHeight));
    m_minIntensityDiffLabel->setText(QString::number(m_params.minIntensityDiff, 'f', 1));
    m_minAspectRatioLabel->setText(QString::number(m_params.minAspectRatio, 'f', 2));
    m_maxAspectRatioLabel->setText(QString::number(m_params.maxAspectRatio, 'f', 2));
    m_minSolidityLabel->setText(QString::number(m_params.minSolidity, 'f', 2));
    m_minCompactnessLabel->setText(QString::number(m_params.minCompactness, 'f', 2));
    
    emit paramsChanged(m_params);
}
