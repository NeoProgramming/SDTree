#pragma once

#include <QWidget>
#include <QImage>
#include <QPoint>

// Canvas for mask editing. First iteration: shows the reference image only.
// Painting, brush and mask overlay are added in the next iteration.
class MaskCanvas : public QWidget
{
    Q_OBJECT
public:
    explicit MaskCanvas(QWidget *parent = nullptr);

    void setImage(const QImage &image);   // reference (init) image
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QImage m_image;   // reference image, shown as-is (1:1 pixels)
};
