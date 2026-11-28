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
#include <QShortcut>

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
    connect(saveBtn, &QPushButton::clicked, 
		this, &MaskEditorDialog::onSaveClicked);
    connect(cancelBtn, &QPushButton::clicked, 
		this, &MaskEditorDialog::reject);

	// --- connect widgets to canvas ---
	connect(m_innerSpin, QOverload<int>::of(&QSpinBox::valueChanged),
		this, [this](int v) {
		if (v >= m_outerSpin->value())
			m_outerSpin->setValue(v + 1);
		m_canvas->setBrushRadius(m_innerSpin->value(), m_outerSpin->value());
	});
	connect(m_outerSpin, QOverload<int>::of(&QSpinBox::valueChanged),
		this, [this](int v) {
		if (v <= m_innerSpin->value())
			m_innerSpin->setValue(qMax(0, v - 1));
		m_canvas->setBrushRadius(m_innerSpin->value(), m_outerSpin->value());
	});
	connect(m_whiteRadio, &QRadioButton::toggled,
		this, [this](bool checked) {
		if (checked)
			m_canvas->setEraser(false);
	});
	connect(m_blackRadio, &QRadioButton::toggled,
		this, [this](bool checked) {
		if (checked)
			m_canvas->setEraser(true);
	});
	connect(m_opacitySlider, &QSlider::valueChanged,
		this, [this](int v) {
		updateOpacityLabel();
		m_canvas->setOpacity(v / 100.0);
	});
}

void MaskEditorDialog::loadImages(const QImage &reference, const QImage &mask)
{
    m_reference = reference;

	if (reference.isNull()) {
		qWarning("MaskEditorDialog::loadImages: reference image is null");
		return;
	}
    
	if (mask.isNull() || mask.size() != reference.size()) {
        // reset mask to black
        m_mask = QImage(reference.size(), QImage::Format_Grayscale8);
        m_mask.fill(0);
    } else {
        m_mask = mask.convertToFormat(QImage::Format_Grayscale8);
    }

    m_canvas->setBaseImage(m_reference); 
	m_canvas->setMaskImage(m_mask);
	m_canvas->setBrushRadius(m_innerSpin->value(), m_outerSpin->value());
	m_canvas->setEraser(!m_whiteRadio->isChecked());
	m_canvas->setOpacity(m_opacitySlider->value() / 100.0);
}

QImage MaskEditorDialog::mask() const
{
    return m_mask;
}

void MaskEditorDialog::onSaveClicked()
{
	m_mask = m_canvas->maskImage();
	if (!m_mask.isNull())
		emit maskSaved(m_mask);
    accept();
}

void MaskEditorDialog::updateOpacityLabel()
{
    m_opacityLabel->setText(QString("%1%").arg(m_opacitySlider->value()));
}
