#include "maskcanvas.h"

#include <QPainter>

MaskCanvas::MaskCanvas(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAutoFillBackground(false);
    setMinimumSize(200, 200);
}

void MaskCanvas::setImage(const QImage &image)
{
    m_image = image;
    if (!m_image.isNull()) {
        setFixedSize(m_image.size());   // 1:1, no scaling
    }
    updateGeometry();
    update();
}

QSize MaskCanvas::sizeHint() const
{
    if (!m_image.isNull())
        return m_image.size();
    return QSize(512, 512);
}

void MaskCanvas::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    if (m_image.isNull()) {
        p.fillRect(rect(), Qt::darkGray);
        p.setPen(Qt::white);
        p.drawText(rect(), Qt::AlignCenter, "No reference image");
        return;
    }
    // draw reference at 1:1
    p.drawImage(0, 0, m_image);
}