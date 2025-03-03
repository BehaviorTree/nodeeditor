#include "ConnectionGraphicsObject.hpp"

#include <QtWidgets/QGraphicsSceneMouseEvent>
#include <QtWidgets/QGraphicsDropShadowEffect>
#include <QtWidgets/QGraphicsBlurEffect>
#include <QtWidgets/QStyleOptionGraphicsItem>
#include <QtWidgets/QGraphicsView>

#include <QtCore/QDebug>

#include "AbstractGraphModel.hpp"
#include "BasicGraphicsScene.hpp"
#include "ConnectionIdUtils.hpp"
#include "ConnectionPainter.hpp"
#include "ConnectionState.hpp"
#include "ConnectionStyle.hpp"
#include "NodeConnectionInteraction.hpp"
#include "NodeGeometry.hpp"
#include "NodeGraphicsObject.hpp"
#include "StyleCollection.hpp"
#include "locateNode.hpp"


namespace QtNodes
{

ConnectionGraphicsObject::
ConnectionGraphicsObject(BasicGraphicsScene &scene,
                         ConnectionId const  connectionId)
  : _connectionId(connectionId)
  , _graphModel(scene.graphModel())
  , _connectionState(*this)
  , _out{0, 0}
  , _in{0, 0}
{
  scene.addItem(this);

  setFlag(QGraphicsItem::ItemIsMovable,    true);
  setFlag(QGraphicsItem::ItemIsFocusable,  true);
  setFlag(QGraphicsItem::ItemIsSelectable, true);

  setAcceptHoverEvents(true);

  //addGraphicsEffect();

  setZValue(-1.0);

  initializePosition();
}


void
ConnectionGraphicsObject::
initializePosition()
{
  // This function is only called when the ConnectionGraphicsObject
  // is newly created. At this moment both end coordinates are (0, 0)
  // in Connection G.O. coordinates. The position of the whole
  // Connection G. O. in scene coordinate system is also (0, 0).
  // By moving the whole object to the Node Port position
  // we position both connection ends correctly.

  if (_connectionState.requiredPort() != PortType::None)
  {
    PortType attachedPort = oppositePort(_connectionState.requiredPort());

    PortIndex portIndex = getPortIndex(attachedPort, _connectionId);
    NodeId nodeId = getNodeId(attachedPort, _connectionId);

    NodeGraphicsObject * ngo =
      nodeScene()->nodeGraphicsObject(nodeId);

    if (ngo)
    {
      QTransform nodeSceneTransform =
        ngo->sceneTransform();

      NodeGeometry geometry(*ngo);

      QPointF pos =
        geometry.portScenePosition(attachedPort,
                                   portIndex,
                                   nodeSceneTransform);

      this->setPos(pos);
    }
  }

  move();
}


AbstractGraphModel &
ConnectionGraphicsObject::
graphModel() const
{
  return _graphModel;
}


BasicGraphicsScene *
ConnectionGraphicsObject::
nodeScene() const
{
  return dynamic_cast<BasicGraphicsScene*>(scene());
}


ConnectionId const &
ConnectionGraphicsObject::
connectionId() const
{
  return _connectionId;
}



QRectF
ConnectionGraphicsObject::
boundingRect() const
{
  auto points = pointsC1C2();

  // `normalized()` fixes inverted rects.
  QRectF basicRect = QRectF(_out, _in).normalized();

  QRectF c1c2Rect = QRectF(points.first, points.second).normalized();

  QRectF commonRect = basicRect.united(c1c2Rect);

  auto const &connectionStyle = StyleCollection::connectionStyle();
  float const diam = connectionStyle.pointDiameter();
  QPointF const cornerOffset(diam, diam);

  // Expand rect by port circle diameter
  commonRect.setTopLeft(commonRect.topLeft() - cornerOffset);
  commonRect.setBottomRight(commonRect.bottomRight() + 2 * cornerOffset);

  return commonRect;
}


QPainterPath
ConnectionGraphicsObject::
shape() const
{
#ifdef DEBUG_DRAWING

  //QPainterPath path;

  //path.addRect(boundingRect());
  //return path;

#else
  return ConnectionPainter::getPainterStroke(*this);
#endif
}


QPointF const &
ConnectionGraphicsObject::
endPoint(PortType portType) const
{
  Q_ASSERT(portType != PortType::None);

  return (portType == PortType::Out ?
          _out :
          _in);
}


void
ConnectionGraphicsObject::
setEndPoint(PortType portType, QPointF const &point)
{
  if (portType == PortType::In)
    _in = point;
  else
    _out = point;
}


void
ConnectionGraphicsObject::
move()
{
  auto moveEnd =
    [this](ConnectionId cId, PortType portType)
    {
      NodeId nodeId = getNodeId(portType, cId);

      if (nodeId == InvalidNodeId)
        return;

      NodeGraphicsObject * ngo =
        nodeScene()->nodeGraphicsObject(nodeId);

      if (ngo)
      {
        NodeGeometry nodeGeometry(*ngo);

        QPointF scenePos =
          nodeGeometry.portScenePosition(portType,
                                         getPortIndex(portType, cId),
                                         ngo->sceneTransform());

        QPointF connectionPos = sceneTransform().inverted().map(scenePos);

        setEndPoint(portType, connectionPos);
      }
    };

  moveEnd(_connectionId, PortType::Out);
  moveEnd(_connectionId, PortType::In);

  prepareGeometryChange();

  update();
}

void ConnectionGraphicsObject::lock(bool locked)
{
  setFlag(QGraphicsItem::ItemIsMovable, !locked);
  setFlag(QGraphicsItem::ItemIsFocusable, !locked);
  setFlag(QGraphicsItem::ItemIsSelectable, !locked);
}

ConnectionState const &
ConnectionGraphicsObject::
connectionState() const
{
  return _connectionState;
}


ConnectionState &
ConnectionGraphicsObject::
connectionState()
{
  return _connectionState;
}

ConnectionStyle ConnectionGraphicsObject::connectionStyle() const
{
  auto connectionStyle = StyleCollection::connectionStyle();

  //-------------------------------------------
  // This is a BehaviorTree specific code, not applicable to upstream

  const auto& defaultStyle = StyleCollection::nodeStyle();

  auto inNodeStyle = _graphModel.nodeData(_connectionId.inNodeId, NodeRole::Style);
  QJsonDocument json = QJsonDocument::fromVariant(inNodeStyle);
  NodeStyle nodeStyle(json.object());

  // In real-time monitoring mode, the color of the connection should be the same
  // as the NormalBoundaryColor.
  // We recognize this case by the fact that nodeStyle and defaultStyle are different
  if(defaultStyle.NormalBoundaryColor != nodeStyle.NormalBoundaryColor)
  {
    auto connectionJson = connectionStyle.toJson();
    auto connectionJsonObj = connectionJson["ConnectionStyle"].toObject();
    connectionJsonObj["NormalColor"] = nodeStyle.NormalBoundaryColor.name();
    connectionJson["ConnectionStyle"] = connectionJsonObj;
    connectionStyle.loadJson(connectionJson);
  }
  //-------------------------------------------

  return connectionStyle;
}

void
ConnectionGraphicsObject::
paint(QPainter * painter,
      QStyleOptionGraphicsItem const * option,
      QWidget *)
{
  if (!scene())
    return;

  painter->setClipRect(option->exposedRect);

  ConnectionPainter::paint(painter, *this);
}


void
ConnectionGraphicsObject::
mousePressEvent(QGraphicsSceneMouseEvent * event)
{
  QGraphicsItem::mousePressEvent(event);
}


void
ConnectionGraphicsObject::
mouseMoveEvent(QGraphicsSceneMouseEvent * event)
{
  prepareGeometryChange();

  auto view = static_cast<QGraphicsView *>(event->widget());
  auto ngo  = locateNodeAt(event->scenePos(),
                           *nodeScene(),
                           view->transform());
  if (ngo)
  {
    ngo->reactToConnection(this);

    _connectionState.setLastHoveredNode(ngo->nodeId());
  }
  else
  {
    _connectionState.resetLastHoveredNode();
  }

  //-------------------

  auto requiredPort = _connectionState.requiredPort();

  if (requiredPort != PortType::None)
  {
    setEndPoint(requiredPort, event->pos());
  }

  //-------------------

  update();

  event->accept();
}


void
ConnectionGraphicsObject::
mouseReleaseEvent(QGraphicsSceneMouseEvent * event)
{
  QGraphicsItem::mouseReleaseEvent(event);

  ungrabMouse();
  event->accept();

  auto view = static_cast<QGraphicsView *>(event->widget());

  Q_ASSERT(view);

  auto ngo = locateNodeAt(event->scenePos(),
                          *nodeScene(),
                          view->transform());

  bool wasConnected = false;

  if (ngo)
  {
    NodeConnectionInteraction interaction(*ngo, *this, *nodeScene());

    wasConnected = interaction.tryConnect();
  }

  // If connection attempt was unsuccessful
  if (!wasConnected)
  {
    // Resulting unique_ptr is not used and automatically deleted.
    nodeScene()->resetDraftConnection();
  }
}


void
ConnectionGraphicsObject::
hoverEnterEvent(QGraphicsSceneHoverEvent * event)
{
  _connectionState.setHovered(true);

  update();

  // Signal
  nodeScene()->connectionHovered(connectionId(), event->screenPos());

  event->accept();
}


void
ConnectionGraphicsObject::
hoverLeaveEvent(QGraphicsSceneHoverEvent * event)
{
  _connectionState.setHovered(false);

  update();

  // Signal
  nodeScene()->connectionHoverLeft(connectionId());

  event->accept();
}


std::pair<QPointF, QPointF>
ConnectionGraphicsObject::
pointsC1C2() const
{
  auto const layout = graphModel().portLayout();
  const double maxOffset = 200;
  const double minOffset = 40;

  double distance = ( layout == PortLayout::Horizontal ) ?
    (_in.x() - _out.x()) : (_in.y() - _out.y());

  double ratio = (distance <= 0) ? 1.0 : 0.4;
  double offset = std::abs(distance) * ratio;
  offset = std::clamp( offset, minOffset, maxOffset);

  if( layout == PortLayout::Horizontal)
  {
    QPointF c1(_out.x() + offset, _out.y());
    QPointF c2(_in.x() - offset, _in.y());
    return std::make_pair(c1, c2);
  }
  else {
    QPointF c1(_out.x(), _out.y() + offset);
    QPointF c2(_in.x(), _in.y() - offset);
    return std::make_pair(c1, c2);
  }
}


void
ConnectionGraphicsObject::
addGraphicsEffect()
{
  auto effect = new QGraphicsBlurEffect;

  effect->setBlurRadius(5);
  setGraphicsEffect(effect);

  //auto effect = new QGraphicsDropShadowEffect;
  //auto effect = new ConnectionBlurEffect(this);
  //effect->setOffset(4, 4);
  //effect->setColor(QColor(Qt::gray).darker(800));
}

void
ConnectionGraphicsObject::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
  if (!flags().testFlag(QGraphicsItem::ItemIsSelectable))
  {
    return;
  }

  Q_EMIT nodeScene()->connectionContextMenu(_connectionId, mapToScene(event->pos()));
}

}
