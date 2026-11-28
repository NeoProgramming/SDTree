#pragma once

#include <QWidget>
#include <QImage>
#include <QPoint>
#include <QPointF>

class MaskCanvas : public QWidget
{
	Q_OBJECT
public:
	explicit MaskCanvas(QWidget *parent = nullptr);

	void setBaseImage(const QImage &img);
	void setMaskImage(const QImage &mask);   // same size as base
	QImage maskImage() const { return m_maskImage; }

	void setBrushRadius(int inner, int outer);
	void setEraser(bool eraser);
	void setOpacity(double opacity);  // 0.0..1.0
		
	QSize sizeHint() const override;

signals:
	void maskChanged();

protected:
	void paintEvent(QPaintEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
	void leaveEvent(QEvent *event) override;
		
private:
	void drawBrushStamp(const QPointF &center);
		
	QImage m_baseImage;
	QImage m_maskImage;         // Format_Grayscale8
	
	int m_rInner = 50;
	int m_rOuter = 100;
	bool m_eraser = true;
	double m_opacity = 0.5;

	bool m_drawing = false;
	QPointF m_cursorPos;
	QPointF m_lastPos;
	bool m_cursorInside = false;
};