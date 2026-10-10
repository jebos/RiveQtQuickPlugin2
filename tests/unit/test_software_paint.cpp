#include <QtTest/QTest>

#include <QImage>
#include <QLinearGradient>
#include <QPainter>

#include "qpainterrivepaint.h"
#include "qpainterriveshader.h"

class SoftwarePaintTest : public QObject
{
    Q_OBJECT

private:
    static QImage render(const QPainterRivePaint& paint)
    {
        QImage image(32, 4, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        painter.fillRect(image.rect(), paint.brush());
        return image;
    }

private slots:
    void gradientTransformIsPerPaint_data()
    {
        QTest::addColumn<bool>("transformFirst");
        QTest::newRow("shader-first") << false;
        QTest::newRow("transform-first") << true;
    }

    void gradientTransformIsPerPaint()
    {
        QFETCH(bool, transformFirst);
        QLinearGradient gradient(0, 0, 8, 0);
        gradient.setColorAt(0, Qt::red);
        gradient.setColorAt(1, Qt::blue);
        auto shader = rive::make_rcp<QPainterRiveShader>(QBrush(gradient));

        QPainterRivePaint transformed;
        const rive::Mat2D matrix(2, 0, 0, 1, 8, 0);
        if (transformFirst) {
            transformed.shaderTransform(matrix);
        }
        transformed.shader(shader);
        if (!transformFirst) {
            transformed.shaderTransform(matrix);
        }

        QPainterRivePaint original;
        original.shader(shader);
        const QImage transformedImage = render(transformed);
        const QImage originalImage = render(original);
        QVERIFY(transformedImage.pixelColor(9, 1).red() > 200);
        QVERIFY(transformedImage.pixelColor(21, 1).blue() > 200);
        QCOMPARE(originalImage.pixelColor(9, 1), QColor(Qt::blue));
        QCOMPARE(shader->brush().transform(), QTransform());

        transformed.shaderTransform(rive::Mat2D());
        QCOMPARE(render(transformed), originalImage);

        transformed.shaderTransform(matrix);
        transformed.shader(nullptr);
        transformed.color(0xff00ff00);
        QCOMPARE(render(transformed).pixelColor(9, 1), QColor(Qt::green));
    }
};

QTEST_GUILESS_MAIN(SoftwarePaintTest)
#include "test_software_paint.moc"
