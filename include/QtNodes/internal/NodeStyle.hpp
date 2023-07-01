#pragma once

#include <QtGui/QColor>

#include "Export.hpp"
#include "Style.hpp"

namespace QtNodes {

class NODE_EDITOR_PUBLIC NodeStyle : public Style
{
public:
    NodeStyle();

    NodeStyle(const NodeStyle& other) = default;

    NodeStyle(QVariant const& variant);

    NodeStyle(QString jsonText);

    NodeStyle(QJsonObject const &json);

    virtual ~NodeStyle() = default;

    NodeStyle& operator=(const NodeStyle& other) noexcept;

public:
    static void setNodeStyle(QString jsonText);

public:
    void loadJson(QJsonObject const &json) override;

    QJsonObject toJson() const override;

    void fromVariantMap(QVariantMap const& map);

    QVariantMap toVariantMap() const;

public:
    bool DashedBoundary;

    QColor NormalBoundaryColor;
    QColor SelectedBoundaryColor;
    QColor GradientColor0;
    QColor GradientColor1;
    QColor GradientColor2;
    QColor GradientColor3;
    QColor ShadowColor;
    QColor FontColor;
    QColor FontColorFaded;

    QColor ConnectionPointColor;
    QColor FilledConnectionPointColor;

    QColor WarningColor;
    QColor ErrorColor;

    float PenWidth;
    float HoveredPenWidth;

    float ConnectionPointDiameter;

    float Opacity;
};

} // namespace QtNodes

Q_DECLARE_METATYPE(QtNodes::NodeStyle);
