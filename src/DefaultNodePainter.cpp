#include "DefaultNodePainter.hpp"

#include <cmath>

#include <QtCore/QMargins>

#include "AbstractGraphModel.hpp"
#include "AbstractNodeGeometry.hpp"
#include "BasicGraphicsScene.hpp"
#include "ConnectionGraphicsObject.hpp"
#include "ConnectionIdUtils.hpp"
#include "NodeGraphicsObject.hpp"
#include "NodeState.hpp"
#include "StyleCollection.hpp"

namespace QtNodes {

void DefaultNodePainter::paint(QPainter *painter, NodeGraphicsObject &ngo) const
{
    // TODO?
    //AbstractNodeGeometry & geometry = ngo.nodeScene()->nodeGeometry();
    //geometry.recomputeSizeIfFontChanged(painter->font());

    drawNodeRect(painter, ngo);

    drawConnectionPoints(painter, ngo);

    drawFilledConnectionPoints(painter, ngo);

    drawNodeCaption(painter, ngo);

    drawEntryLabels(painter, ngo);

    drawResizeRect(painter, ngo);
}

void DefaultNodePainter::drawNodeRect(QPainter *painter, NodeGraphicsObject &ngo) const
{
    AbstractGraphModel &model = ngo.graphModel();

    NodeId const nodeId = ngo.nodeId();

    AbstractNodeGeometry &geometry = ngo.nodeScene()->nodeGeometry();

    QSize size = geometry.size(nodeId);

    const QVariant style = model.nodeData(nodeId, NodeRole::Style);
    NodeStyle nodeStyle(style);

    if (nodeStyle.GradientColor0 == nodeStyle.GradientColor1
        && nodeStyle.GradientColor0 == nodeStyle.GradientColor2
        && nodeStyle.GradientColor0 == nodeStyle.GradientColor3) {
        painter->setBrush(nodeStyle.GradientColor0);
    } else {
        QLinearGradient gradient(QPointF(0.0, 0.0), QPointF(2.0, size.height()));
        gradient.setColorAt(0.0, nodeStyle.GradientColor0);
        gradient.setColorAt(0.10, nodeStyle.GradientColor1);
        gradient.setColorAt(0.90, nodeStyle.GradientColor2);
        gradient.setColorAt(1.0, nodeStyle.GradientColor3);
        painter->setBrush(gradient);
    }

    QRectF boundary(0, 0, size.width(), size.height());
    const float width = ngo.nodeState().hovered() ? nodeStyle.HoveredPenWidth : nodeStyle.PenWidth;
    const double radius = 2.5 + nodeStyle.PenWidth / 2;
    const QColor color = ngo.isSelected() ? nodeStyle.SelectedBoundaryColor
                                          : nodeStyle.NormalBoundaryColor;

    QPen p(color, width);
    p.setJoinStyle(Qt::RoundJoin);
    painter->setPen(p);
    painter->drawRoundedRect(boundary, radius, radius);

    if (nodeStyle.DashedBoundaryColor.alpha() != 0) {
        const qreal dashWidth = nodeStyle.PenWidth * 2;
        const QColor dashColor = nodeStyle.DashedBoundaryColor;
        painter->save();
        QPen dashPen(dashColor, dashWidth);
        dashPen.setStyle(Qt::PenStyle::DashLine);
        dashPen.setCapStyle(Qt::FlatCap);
        dashPen.setJoinStyle(Qt::RoundJoin);
        dashPen.setDashPattern({4, 3});
        painter->setPen(dashPen);
        auto margin = dashWidth * 1.5;
        painter->drawRoundedRect(boundary.marginsRemoved({margin, margin, margin, margin}),
                                 radius,
                                 radius);
        painter->restore();
    }

    const auto flags = model.nodeFlags(nodeId);
    if (flags.testFlag(SearchMatched)) {
        painter->save();
        painter->setPen(QPen(nodeStyle.WarningColor, width));
        painter->setBrush(nodeStyle.WarningColor);
        QPolygonF triangle;
        triangle.push_back({0, 0});
        triangle.push_back({0, 18});
        triangle.push_back({18, 0});
        painter->drawConvexPolygon(triangle);
        painter->restore();
    }
}

void DefaultNodePainter::drawConnectionPoints(QPainter *painter, NodeGraphicsObject &ngo) const
{
    AbstractGraphModel &model = ngo.graphModel();
    NodeId const nodeId = ngo.nodeId();
    AbstractNodeGeometry &geometry = ngo.nodeScene()->nodeGeometry();

    const QVariant style = model.nodeData(nodeId, NodeRole::Style);
    NodeStyle nodeStyle(style);

    auto const &connectionStyle = StyleCollection::connectionStyle();

    float diameter = nodeStyle.ConnectionPointDiameter;
    auto reducedDiameter = diameter * 0.6;

    for (PortType portType : {PortType::Out, PortType::In}) {
        size_t const n = model
                             .nodeData(nodeId,
                                       (portType == PortType::Out) ? NodeRole::OutPortCount
                                                                   : NodeRole::InPortCount)
                             .toUInt();

        for (PortIndex portIndex = 0; portIndex < n; ++portIndex) {
            QPointF p = geometry.portPosition(nodeId, portType, portIndex);

            auto const &dataType = model.portData(nodeId, portType, portIndex, PortRole::DataType)
                                       .value<NodeDataType>();

            double r = 1.0;

            NodeState const &state = ngo.nodeState();

            if (auto const *cgo = state.connectionForReaction()) {
                PortType requiredPort = cgo->connectionState().requiredPort();

                if (requiredPort == portType) {
                    ConnectionId possibleConnectionId = makeCompleteConnectionId(cgo->connectionId(),
                                                                                 nodeId,
                                                                                 portIndex);

                    bool const possible = model.connectionPossible(possibleConnectionId);

                    auto cp = cgo->sceneTransform().map(cgo->endPoint(requiredPort));
                    cp = ngo.sceneTransform().inverted().map(cp);

                    auto diff = cp - p;
                    double dist = std::sqrt(QPointF::dotProduct(diff, diff));

                    if (possible) {
                        double const thres = 40.0;
                        r = (dist < thres) ? (2.0 - dist / thres) : 1.0;
                    } else {
                        double const thres = 80.0;
                        r = (dist < thres) ? (dist / thres) : 1.0;
                    }
                }
            }

            if (connectionStyle.useDataDefinedColors()) {
                painter->setBrush(connectionStyle.normalColor(dataType.id));
            } else {
                painter->setBrush(nodeStyle.ConnectionPointColor);
            }

            painter->drawEllipse(p, reducedDiameter * r, reducedDiameter * r);
        }
    }

    if (ngo.nodeState().connectionForReaction()) {
        ngo.nodeState().resetConnectionForReaction();
    }
}

void DefaultNodePainter::drawFilledConnectionPoints(QPainter *painter, NodeGraphicsObject &ngo) const
{
    AbstractGraphModel &model = ngo.graphModel();
    NodeId const nodeId = ngo.nodeId();
    AbstractNodeGeometry &geometry = ngo.nodeScene()->nodeGeometry();

    const QVariant style = model.nodeData(nodeId, NodeRole::Style);
    NodeStyle nodeStyle(style);

    auto diameter = nodeStyle.ConnectionPointDiameter;

    for (PortType portType : {PortType::Out, PortType::In}) {
        size_t const n = model
                             .nodeData(nodeId,
                                       (portType == PortType::Out) ? NodeRole::OutPortCount
                                                                   : NodeRole::InPortCount)
                             .toUInt();

        for (PortIndex portIndex = 0; portIndex < n; ++portIndex) {
            QPointF p = geometry.portPosition(nodeId, portType, portIndex);

            auto const &connected = model.connections(nodeId, portType, portIndex);

            if (!connected.empty()) {
                auto const &dataType = model
                                           .portData(nodeId, portType, portIndex, PortRole::DataType)
                                           .value<NodeDataType>();

                auto const &connectionStyle = StyleCollection::connectionStyle();
                if (connectionStyle.useDataDefinedColors()) {
                    QColor const c = connectionStyle.normalColor(dataType.id);
                    painter->setPen(c);
                    painter->setBrush(c);
                } else {
                    painter->setPen(nodeStyle.FilledConnectionPointColor);
                    painter->setBrush(nodeStyle.FilledConnectionPointColor);
                }

                painter->drawEllipse(p, diameter * 0.4, diameter * 0.4);
            }
        }
    }
}

void DefaultNodePainter::drawNodeCaption(QPainter *painter, NodeGraphicsObject &ngo) const
{
    AbstractGraphModel &model = ngo.graphModel();
    NodeId const nodeId = ngo.nodeId();
    AbstractNodeGeometry &geometry = ngo.nodeScene()->nodeGeometry();

    if (!model.nodeData(nodeId, NodeRole::CaptionVisible).toBool())
        return;

    QString const name = model.nodeData(nodeId, NodeRole::Caption).toString();

    QFont f = painter->font();
    f.setBold(true);

    QPointF position = geometry.captionPosition(nodeId);

    const QVariant style = model.nodeData(nodeId, NodeRole::Style);
    NodeStyle nodeStyle(style);

    painter->setFont(f);
    painter->setPen(nodeStyle.FontColor);
    painter->drawText(position, name);

    f.setBold(false);
    painter->setFont(f);
}

void DefaultNodePainter::drawEntryLabels(QPainter *painter, NodeGraphicsObject &ngo) const
{
    AbstractGraphModel &model = ngo.graphModel();
    NodeId const nodeId = ngo.nodeId();
    AbstractNodeGeometry &geometry = ngo.nodeScene()->nodeGeometry();

    const QVariant style = model.nodeData(nodeId, NodeRole::Style);
    NodeStyle nodeStyle(style);

    for (PortType portType : {PortType::Out, PortType::In}) {
        unsigned int n = model.nodeData<unsigned int>(nodeId,
                                                      (portType == PortType::Out)
                                                          ? NodeRole::OutPortCount
                                                          : NodeRole::InPortCount);

        for (PortIndex portIndex = 0; portIndex < n; ++portIndex) {
            auto const &connected = model.connections(nodeId, portType, portIndex);

            QPointF p = geometry.portTextPosition(nodeId, portType, portIndex);

            if (connected.empty())
                painter->setPen(nodeStyle.FontColorFaded);
            else
                painter->setPen(nodeStyle.FontColor);

            QString s;

            if (model.portData<bool>(nodeId, portType, portIndex, PortRole::CaptionVisible)) {
                s = model.portData<QString>(nodeId, portType, portIndex, PortRole::Caption);
            } else {
                auto portData = model.portData(nodeId, portType, portIndex, PortRole::DataType);

                s = portData.value<NodeDataType>().name;
            }

            painter->drawText(p, s);
        }
    }
}

void DefaultNodePainter::drawResizeRect(QPainter *painter, NodeGraphicsObject &ngo) const
{
    AbstractGraphModel &model = ngo.graphModel();
    NodeId const nodeId = ngo.nodeId();
    AbstractNodeGeometry &geometry = ngo.nodeScene()->nodeGeometry();

    if (model.nodeFlags(nodeId) & NodeFlag::Resizable) {
        painter->setBrush(Qt::gray);

        painter->drawEllipse(geometry.resizeHandleRect(nodeId));
    }
}

} // namespace QtNodes
