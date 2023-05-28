#include "NodeStyle.hpp"

#include <iostream>

#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonValueRef>

#include <QtCore/QDebug>

#include "StyleCollection.hpp"

using QtNodes::NodeStyle;

inline void initResources()
{
    Q_INIT_RESOURCE(resources);
}

NodeStyle::NodeStyle()
{
    // Explicit resources inialization for preventing the static initialization
    // order fiasco: https://isocpp.org/wiki/faq/ctors#static-init-order
    initResources();

    // This configuration is stored inside the compiled unit and is loaded statically
    loadJsonFile(":DefaultStyle.json");
}

NodeStyle::NodeStyle(const QVariant &style)
{
    if(style.userType() == QMetaType::QVariantMap)
    {
        loadJson(style.toJsonObject());
    }
    else *this = style.value<NodeStyle>();
}

NodeStyle::NodeStyle(QString jsonText)
{
    loadJsonText(jsonText);
}

NodeStyle::NodeStyle(QJsonObject const &json)
{
    loadJson(json);
}

NodeStyle &NodeStyle::operator=(const NodeStyle &other) noexcept
{
    NormalBoundaryColor = other.NormalBoundaryColor;
    SelectedBoundaryColor = other.SelectedBoundaryColor;
    GradientColor0 = other.GradientColor0;
    GradientColor1 = other.GradientColor1;
    GradientColor2 = other.GradientColor2;
    GradientColor3 = other.GradientColor3;
    ShadowColor = other.ShadowColor;
    FontColor = other.FontColor;
    FontColorFaded = other.FontColorFaded;
    ConnectionPointColor = other.ConnectionPointColor;
    FilledConnectionPointColor = other.FilledConnectionPointColor;
    WarningColor = other.WarningColor;
    ErrorColor = other.ErrorColor;

    PenWidth = other.PenWidth;
    HoveredPenWidth = other.HoveredPenWidth;
    ConnectionPointDiameter = other.ConnectionPointDiameter;
    Opacity = other.Opacity;

    return *this;
}

void NodeStyle::setNodeStyle(QString jsonText)
{
    NodeStyle style(jsonText);

    StyleCollection::setNodeStyle(style);
}

#ifdef STYLE_DEBUG
#define NODE_STYLE_CHECK_UNDEFINED_VALUE(v, variable) \
    { \
        if (v.type() == QJsonValue::Undefined || v.type() == QJsonValue::Null) \
            qWarning() << "Undefined value for parameter:" << #variable; \
    }
#else
#define NODE_STYLE_CHECK_UNDEFINED_VALUE(v, variable)
#endif

#define NODE_STYLE_READ_COLOR(values, variable) \
    { \
        auto valueRef = values[#variable]; \
        NODE_STYLE_CHECK_UNDEFINED_VALUE(valueRef, variable) \
        if (valueRef.isArray()) { \
            auto colorArray = valueRef.toArray(); \
            std::vector<int> rgb; \
            rgb.reserve(3); \
            for (auto it = colorArray.begin(); it != colorArray.end(); ++it) { \
                rgb.push_back((*it).toInt()); \
            } \
            variable = QColor(rgb[0], rgb[1], rgb[2]); \
        } else { \
            variable = QColor(valueRef.toString()); \
        } \
    }

#define NODE_STYLE_WRITE_COLOR(values, variable) \
    { \
        values[#variable] = variable.name(QColor::NameFormat::HexArgb); \
    }

#define NODE_STYLE_READ_FLOAT(values, variable) \
    { \
        auto valueRef = values[#variable]; \
        NODE_STYLE_CHECK_UNDEFINED_VALUE(valueRef, variable) \
        variable = valueRef.toDouble(); \
    }

#define NODE_STYLE_WRITE_FLOAT(values, variable) \
    { \
        values[#variable] = variable; \
    }

void NodeStyle::loadJson(QJsonObject const &json)
{
    QJsonValue nodeStyleValues = json["NodeStyle"];

    QJsonObject obj = nodeStyleValues.toObject();

    NODE_STYLE_READ_COLOR(obj, NormalBoundaryColor);
    NODE_STYLE_READ_COLOR(obj, SelectedBoundaryColor);
    NODE_STYLE_READ_COLOR(obj, GradientColor0);
    NODE_STYLE_READ_COLOR(obj, GradientColor1);
    NODE_STYLE_READ_COLOR(obj, GradientColor2);
    NODE_STYLE_READ_COLOR(obj, GradientColor3);
    NODE_STYLE_READ_COLOR(obj, ShadowColor);
    NODE_STYLE_READ_COLOR(obj, FontColor);
    NODE_STYLE_READ_COLOR(obj, FontColorFaded);
    NODE_STYLE_READ_COLOR(obj, ConnectionPointColor);
    NODE_STYLE_READ_COLOR(obj, FilledConnectionPointColor);
    NODE_STYLE_READ_COLOR(obj, WarningColor);
    NODE_STYLE_READ_COLOR(obj, ErrorColor);

    NODE_STYLE_READ_FLOAT(obj, PenWidth);
    NODE_STYLE_READ_FLOAT(obj, HoveredPenWidth);
    NODE_STYLE_READ_FLOAT(obj, ConnectionPointDiameter);

    NODE_STYLE_READ_FLOAT(obj, Opacity);
}

QJsonObject NodeStyle::toJson() const
{
    QJsonObject obj;

    NODE_STYLE_WRITE_COLOR(obj, NormalBoundaryColor);
    NODE_STYLE_WRITE_COLOR(obj, SelectedBoundaryColor);
    NODE_STYLE_WRITE_COLOR(obj, GradientColor0);
    NODE_STYLE_WRITE_COLOR(obj, GradientColor1);
    NODE_STYLE_WRITE_COLOR(obj, GradientColor2);
    NODE_STYLE_WRITE_COLOR(obj, GradientColor3);
    NODE_STYLE_WRITE_COLOR(obj, ShadowColor);
    NODE_STYLE_WRITE_COLOR(obj, FontColor);
    NODE_STYLE_WRITE_COLOR(obj, FontColorFaded);
    NODE_STYLE_WRITE_COLOR(obj, ConnectionPointColor);
    NODE_STYLE_WRITE_COLOR(obj, FilledConnectionPointColor);
    NODE_STYLE_WRITE_COLOR(obj, WarningColor);
    NODE_STYLE_WRITE_COLOR(obj, ErrorColor);

    NODE_STYLE_WRITE_FLOAT(obj, PenWidth);
    NODE_STYLE_WRITE_FLOAT(obj, HoveredPenWidth);
    NODE_STYLE_WRITE_FLOAT(obj, ConnectionPointDiameter);

    NODE_STYLE_WRITE_FLOAT(obj, Opacity);

    QJsonObject root;
    root["NodeStyle"] = obj;

    return root;
}

void NodeStyle::fromVariantMap(const QVariantMap &map)
{
    NormalBoundaryColor = map.value("NormalBoundaryColor").value<QColor>();
    SelectedBoundaryColor = map.value("SelectedBoundaryColor").value<QColor>();
    GradientColor0 = map.value("GradientColor0").value<QColor>();
    GradientColor1 = map.value("GradientColor1").value<QColor>();
    GradientColor2 = map.value("GradientColor2").value<QColor>();
    GradientColor3 = map.value("GradientColor3").value<QColor>();
    ShadowColor = map.value("ShadowColor").value<QColor>();
    FontColor = map.value("FontColor").value<QColor>();
    FontColorFaded = map.value("FontColorFaded").value<QColor>();

    ConnectionPointColor = map.value("ConnectionPointColor").value<QColor>();
    FilledConnectionPointColor = map.value("FilledConnectionPointColor").value<QColor>();
    WarningColor = map.value("WarningColor").value<QColor>();
    ErrorColor = map.value("ErrorColor").value<QColor>();

    PenWidth = map.value("PenWidth").value<float>();
    HoveredPenWidth = map.value("HoveredPenWidth").value<float>();
    ConnectionPointDiameter = map.value("ConnectionPointDiameter").value<float>();
    Opacity = map.value("Opacity").value<float>();
}

QVariantMap NodeStyle::toVariantMap() const
{
    QVariantMap map;
    map["NormalBoundaryColor"] = NormalBoundaryColor;
    map["SelectedBoundaryColor"] = SelectedBoundaryColor;
    map["GradientColor0"] = GradientColor0;
    map["GradientColor1"] = GradientColor1;
    map["GradientColor2"] = GradientColor2;
    map["GradientColor3"] = GradientColor3;

    map["GradientColor3"] = GradientColor3;
    map["GradientColor3"] = GradientColor3;
    map["FontColorFaded"] = FontColorFaded;
    map["ConnectionPointColor"] = ConnectionPointColor;
    map["FilledConnectionPointColor"] = FilledConnectionPointColor;
    map["WarningColor"] = WarningColor;
    map["ErrorColor"] = ErrorColor;

    map["PenWidth"] = PenWidth;
    map["HoveredPenWidth"] = HoveredPenWidth;
    map["ConnectionPointDiameter"] = ConnectionPointDiameter;
    map["Opacity"] = Opacity;

    return map;
}
