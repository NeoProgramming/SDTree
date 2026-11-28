#include "maskededitordialog.h"
#include "maskcanvas.h"

#include <QScrollArea>
#include <QSpinBox>
#include <QSlider>
#include <QRadioButton>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QButtonGroup>

MaskEditorDialog::MaskEditorDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Mask editor");
    setModal(false);
    resize(900, 700);
    buildUi();
}

void MaskEditorDialog::buildUi()
{
    // --- canvas in scroll area ---
    m_canvas = new MaskCanvas(this);
    m_scroll = new QScrollArea(this);
    m_scroll->setWidget(m_canvas);
    m_scroll->setWidgetResizable(false);       // canvas keeps its own size
    m_scroll->setAlignment(Qt::AlignCenter);
    m_scroll->setBackgroundRole(QPalette::Dark);

    // --- brush radii ---
    m_innerSpin = new QSpinBox(this);
    m_innerSpin->setRange(0, 5000);
    m_innerSpin->setValue(50);

    m_outerSpin = new QSpinBox(this);
    m_outerSpin->setRange(1, 5000);
    m_outerSpin->setValue(100);

    auto *radiiBox = new QGroupBox("Brush radius", this);
    auto *radiiForm = new QFormLayout(radiiBox);
    radiiForm->addRow("Inner:", m_innerSpin);
    radiiForm->addRow("Outer:", m_outerSpin);

    // --- color ---
    m_whiteRadio = new QRadioButton("White", this);
    m_blackRadio = new QRadioButton("Black", this);
    m_whiteRadio->setChecked(true);
    auto *colorGroup = new QButtonGroup(this);
    colorGroup->addButton(m_whiteRadio);
    colorGroup->addButton(m_blackRadio);

    auto *colorBox = new QGroupBox("Brush color", this);
    auto *colorLayout = new QVBoxLayout(colorBox);
    colorLayout->addWidget(m_whiteRadio);
    colorLayout->addWidget(m_blackRadio);

    // --- opacity ---
    m_opacitySlider = new QSlider(Qt::Horizontal, this);
    m_opacitySlider->setRange(0, 100);
    m_opacitySlider->setValue(50);
    m_opacityLabel = new QLabel("50%", this);
    m_opacityLabel->setMinimumWidth(40);

    auto *opacityRow = new QHBoxLayout();
    opacityRow->addWidget(new QLabel("Mask opacity:", this));
    opacityRow->addWidget(m_opacitySlider, 1);
    opacityRow->addWidget(m_opacityLabel);

    // --- buttons ---
    auto *saveBtn = new QPushButton("Save", this);
    auto *cancelBtn = new QPushButton("Cancel", this);
    auto *btnRow = new QHBoxLayout();
    btnRow->addStretch(1);
    btnRow->addWidget(saveBtn);
    btnRow->addWidget(cancelBtn);

    // --- layout ---
    auto *controls = new QVBoxLayout();
    controls->addWidget(radiiBox);
    controls->addWidget(colorBox);
    controls->addLayout(opacityRow);

    auto *bottom = new QHBoxLayout();
    bottom->addLayout(controls, 2);
    bottom->addStretch(1);
    bottom->addLayout(btnRow);

    auto *root = new QVBoxLayout(this);
    root->addWidget(m_scroll, 1);
    root->addLayout(bottom);

    // --- connections ---
    connect(saveBtn, &QPushButton::clicked, this, &MaskEditorDialog::onSaveClicked);
    connect(cancelBtn, &QPushButton::clicked, this, &MaskEditorDialog::reject);
    connect(m_innerSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MaskEditorDialog::onInnerRadiusChanged);
    connect(m_outerSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MaskEditorDialog::onOuterRadiusChanged);
    connect(m_opacitySlider, &QSlider::valueChanged,
            this, &MaskEditorDialog::onOpacityChanged);
    connect(m_whiteRadio, &QRadioButton::toggled,
            this, &MaskEditorDialog::onColorChanged);
    connect(m_blackRadio, &QRadioButton::toggled,
            this, &MaskEditorDialog::onColorChanged);
}

void MaskEditorDialog::loadImages(const QImage &reference, const QImage &mask)
{
    m_reference = reference;

    if (reference.isNull()) {
        m_mask = QImage();
    } else if (mask.isNull() || mask.size() != reference.size()) {
        // reset mask to black
        m_mask = QImage(reference.size(), QImage::Format_Grayscale8);
        m_mask.fill(0);
    } else {
        m_mask = mask.convertToFormat(QImage::Format_Grayscale8);
    }

    m_canvas->setImage(m_reference);
}

QImage MaskEditorDialog::mask() const
{
    return m_mask;
}

void MaskEditorDialog::onSaveClicked()
{
    if (!m_mask.isNull())
        emit maskSaved(m_mask);
    accept();
}

void MaskEditorDialog::onInnerRadiusChanged(int value)
{
    if (value >= m_outerSpin->value())
        m_outerSpin->setValue(value + 1);
    // painting comes in the next iteration
}

void MaskEditorDialog::onOuterRadiusChanged(int value)
{
    if (value <= m_innerSpin->value())
        m_innerSpin->setValue(qMax(0, value - 1));
    // painting comes in the next iteration
}

void MaskEditorDialog::onOpacityChanged(int value)
{
    updateOpacityLabel();
    m_canvas->update();   // opacity not yet used in painting; placeholder
}

void MaskEditorDialog::onColorChanged()
{
    // no-op until painting is implemented
}

void MaskEditorDialog::updateOpacityLabel()
{
    m_opacityLabel->setText(QString("%1%").arg(m_opacitySlider->value()));
}
