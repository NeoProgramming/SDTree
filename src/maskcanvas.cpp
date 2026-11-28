#include "maskcanvas.h"

#include <QPainter>
#include <QMouseEvent>
#include <QRadialGradient>
#include <QPainterPath>
#include <QtMath>

static const double kMaskOnlyThreshold = 0.99;

MaskCanvas::MaskCanvas(QWidget *parent)
	: QWidget(parent)
{
	setAttribute(Qt::WA_OpaquePaintEvent, true);
	setAutoFillBackground(false);
	setMinimumSize(200, 200);
	setMouseTracking(true);
}

void MaskCanvas::setBaseImage(const QImage &img)
{
	m_baseImage = img;
	if (m_maskImage.size() != img.size()) {
		m_maskImage = QImage(img.size(), QImage::Format_Grayscale8);
		m_maskImage.fill(0);   // black = keep original
	}
	setFixedSize(img.size());
	update();
}

void MaskCanvas::setMaskImage(const QImage &mask)
{
	if (m_baseImage.isNull() || mask.size() != m_baseImage.size()) {
		// reset to black of base size
		if (!m_baseImage.isNull()) {
			m_maskImage = QImage(m_baseImage.size(), QImage::Format_Grayscale8);
			m_maskImage.fill(0);
		}
		else {
			m_maskImage = QImage();
		}
	}
	else {
		m_maskImage = mask.convertToFormat(QImage::Format_Grayscale8);
	}
	update();
}

void MaskCanvas::setBrushRadius(int inner, int outer)
{
	m_rInner = std::max(0, inner);
	m_rOuter = std::max(m_rInner + 1, outer);
	update();
}

void MaskCanvas::setEraser(bool eraser)
{
	m_eraser = eraser;
}

void MaskCanvas::setOpacity(double opacity)
{
	m_opacity = std::clamp(opacity, 0.0, 1.0);
	update();
}

void MaskCanvas::paintEvent(QPaintEvent *)
{
	QPainter painter(this);
	painter.fillRect(rect(), QColor(40, 40, 40));

	if (m_baseImage.isNull())
		return;

	painter.setOpacity(1.0);
	painter.drawImage(0, 0, m_baseImage);
	painter.setOpacity(m_opacity);
	painter.drawImage(0, 0, m_maskImage);
	painter.setOpacity(1.0);

	if (m_cursorInside) {
		painter.setBrush(Qt::NoBrush);
		painter.setPen(QPen(Qt::blue, 1, Qt::SolidLine));
		painter.drawEllipse(m_cursorPos, m_rInner, m_rInner);
		painter.setPen(QPen(Qt::red, 1, Qt::SolidLine));
		painter.drawEllipse(m_cursorPos, m_rOuter, m_rOuter);
	}
}

void MaskCanvas::drawBrushStamp(const QPointF &center)
{
	if (m_maskImage.isNull() || m_rOuter <= 0)
		return;

	QPainter p(&m_maskImage);
	p.setRenderHint(QPainter::Antialiasing, true);
	p.setCompositionMode(m_eraser 
		? QPainter::CompositionMode_Darken
		: QPainter::CompositionMode_Lighten);

	const double rOut = m_rOuter;
	const double ratio = std::clamp(double(m_rInner) / rOut, 0.0, 1.0);

	QRadialGradient grad(center, rOut);
	if (m_eraser) {
		grad.setColorAt(0.0, Qt::black);
		grad.setColorAt(ratio, Qt::black);
		grad.setColorAt(1.0, Qt::white);
	}
	else {
		grad.setColorAt(0.0, Qt::white);
		grad.setColorAt(ratio, Qt::white);
		grad.setColorAt(1.0, Qt::black);
	}

	p.setPen(Qt::NoPen);
	p.setBrush(grad);
	p.drawEllipse(center, rOut, rOut);
}

QSize MaskCanvas::sizeHint() const
{
	if (!m_baseImage.isNull())
		return m_baseImage.size();
	return QSize(512, 512);
}


void MaskCanvas::mousePressEvent(QMouseEvent *e)
{
	if (e->button() != Qt::LeftButton || m_baseImage.isNull())
		return;
	m_drawing = true;
	m_lastPos = e->pos();
	drawBrushStamp(m_lastPos);
	update();
	emit maskChanged();
}

void MaskCanvas::mouseMoveEvent(QMouseEvent *e)
{
	m_cursorPos = e->pos();
	m_cursorInside = true;

	if (m_drawing && (e->buttons() & Qt::LeftButton) && !m_maskImage.isNull()) {
		const QPointF cur = e->pos();
		QLineF line(m_lastPos, cur);
		const double len = line.length();
		const double step = std::max(1.0, m_rOuter * 0.1);
		if (len > 0.0) {
			for (double d = 0.0; d <= len; d += step)
				drawBrushStamp(line.pointAt(d / len));
		}
		drawBrushStamp(cur);
		m_lastPos = cur;
		update();
		emit maskChanged();
	}
	else {
		update();
	}
}

void MaskCanvas::mouseReleaseEvent(QMouseEvent *e)
{
	if (e->button() == Qt::LeftButton)
		m_drawing = false;
}

void MaskCanvas::leaveEvent(QEvent *e)
{
	m_cursorInside = false;
	m_drawing = false;
	update();
	QWidget::leaveEvent(e);
}








