#pragma once

#include <QDialog>
#include <QImage>

class MaskCanvas;
class QSpinBox;
class QSlider;
class QRadioButton;
class QLabel;
class QScrollArea;

class MaskEditorDialog : public QDialog
{
    Q_OBJECT
public:
    explicit MaskEditorDialog(QWidget *parent = nullptr);

    // Load reference image and (optionally) existing mask.
    // If mask is null or has different size, it is reset to black.
    void loadImages(const QImage &reference, const QImage &mask = QImage());

    QImage mask() const;

signals:
    // Emitted when user clicks Save. Contains the current mask.
    void maskSaved(const QImage &mask);

private slots:
    void onSaveClicked();
    void onOuterRadiusChanged(int value);
    void onInnerRadiusChanged(int value);
    void onOpacityChanged(int value);
    void onColorChanged();

private:
    void buildUi();
    void updateOpacityLabel();

    MaskCanvas   *m_canvas = nullptr;
    QScrollArea  *m_scroll = nullptr;

    QSpinBox     *m_innerSpin = nullptr;
    QSpinBox     *m_outerSpin = nullptr;

    QRadioButton *m_whiteRadio = nullptr;
    QRadioButton *m_blackRadio = nullptr;

    QSlider      *m_opacitySlider = nullptr;
    QLabel       *m_opacityLabel = nullptr;

    QImage        m_reference;
    QImage        m_mask;
};
