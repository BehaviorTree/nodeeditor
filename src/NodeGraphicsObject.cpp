#include "NodeGraphicsObject.hpp"

#include <iostream>
#include <cstdlib>

#include <QtWidgets/QtWidgets>
#include <QtWidgets/QGraphicsEffect>

#include "AbstractGraphModel.hpp"
#include "BasicGraphicsScene.hpp"
#include "ConnectionGraphicsObject.hpp"
#include "ConnectionIdUtils.hpp"
#include "NodeConnectionInteraction.hpp"
#include "NodeGeometry.hpp"
#include "NodePainter.hpp"
#include "StyleCollection.hpp"
#include "UndoCommands.hpp"


namespace QtNodes
{

NodeGraphicsObject::
NodeGraphicsObject(BasicGraphicsScene& scene,
                   NodeId              nodeId)
  : _nodeId(nodeId)
  , _graphModel(scene.graphModel())
  , _nodeState(*this)
  , _proxyWidget(nullptr)
{
  scene.addItem(this);

  setFlag(QGraphicsItem::ItemDoesntPropagateOpacityToChildren, true);
  setFlag(QGraphicsItem::ItemIsFocusable,                      true);
  setFlag(QGraphicsItem::ItemIsMovable,                        true);
  setFlag(QGraphicsItem::ItemIsSelectable,                     true);
  setFlag(QGraphicsItem::ItemSendsScenePositionChanges,        true);

  setCacheMode(QGraphicsItem::DeviceCoordinateCache);

//  QJsonObject nodeStyleJson =
//    _graphModel.nodeData(_nodeId, NodeRole::Style).toJsonObject();

//  NodeStyle nodeStyle(nodeStyleJson);

  // TODO: Take style from model.
  auto const& nodeStyle = StyleCollection::nodeStyle();
  if(nodeStyle.ShadowColor != Qt::transparent)
  {
    auto effect = new QGraphicsDropShadowEffect;
    effect->setOffset(4, 4);
    effect->setBlurRadius(20);
    effect->setColor(nodeStyle.ShadowColor);

    setGraphicsEffect(effect);
  }

  setOpacity(nodeStyle.Opacity);

  setAcceptHoverEvents(true);

  setZValue(0);

  embedQWidget();

  NodeGeometry geometry(*this);
  geometry.recalculateSize();

  QPointF const pos =
    _graphModel.nodeData(_nodeId, NodeRole::Position).value<QPointF>();

  setPos(pos);
}


AbstractGraphModel &
NodeGraphicsObject::
graphModel() const
{
  return _graphModel;
}


BasicGraphicsScene*
NodeGraphicsObject::
nodeScene() const
{
  return dynamic_cast<BasicGraphicsScene*>(scene());
}


void
NodeGraphicsObject::
embedQWidget()
{
  NodeGeometry geom(*this);

  if (auto w = _graphModel.nodeData(_nodeId,
                                    NodeRole::Widget).value<QWidget*>())
  {
    _proxyWidget = new QGraphicsProxyWidget(this);

    _proxyWidget->setWidget(w);

    _proxyWidget->setPreferredWidth(5);

    NodeGeometry(*this).recalculateSize();

    if (w->sizePolicy().verticalPolicy() & QSizePolicy::ExpandFlag)
    {
      // If the widget wants to use as much vertical space as possible, set
      // it to have the geom's equivalentWidgetHeight.
      _proxyWidget->setMinimumHeight(geom.maxInitialWidgetHeight());
    }

    _proxyWidget->setPos(geom.widgetPosition());

    //update();

    _proxyWidget->setOpacity(1.0);
    _proxyWidget->setFlag(QGraphicsItem::ItemIgnoresParentOpacity);
  }
}


#if 0
void
NodeGraphicsObject::
onNodeSizeUpdated()
{
  if (nodeDataModel()->embeddedWidget())
  {
    nodeDataModel()->embeddedWidget()->adjustSize();
  }
  nodeGeometry().recalculateSize();
  for (PortType type: {PortType::In, PortType::Out})
  {
    for (auto& conn_set : nodeState().getEntries(type))
    {
      for (auto& pair: conn_set)
      {
        Connection* conn = pair.second;
        conn->getConnectionGraphicsObject().move();
      }
    }
  }
}


#endif


QRectF
NodeGraphicsObject::
boundingRect() const
{
  return NodeGeometry(*this).boundingRect();
}


void
NodeGraphicsObject::
setGeometryChanged()
{
  prepareGeometryChange();
}


void
NodeGraphicsObject::
moveConnections() const
{
  auto const& connected = _graphModel.allConnectionIds(_nodeId);

  for (auto& cnId : connected)
  {
    auto cgo = nodeScene()->connectionGraphicsObject(cnId);

    if (cgo)
      cgo->move();
  }
}

void NodeGraphicsObject::onNodeResized()
{
  if (auto w = _graphModel.nodeData(_nodeId, NodeRole::Widget).value<QWidget*>())
  {
    w->adjustSize();

    prepareGeometryChange();

    NodeGeometry geometry(*this);

    geometry.recalculateSize();

    update();

    moveConnections();
  }
}

void
NodeGraphicsObject::
reactToConnection(ConnectionGraphicsObject const* cgo)
{
  _nodeState.storeConnectionForReaction(cgo);

  update();
}

void NodeGraphicsObject::lock(bool locked)
{
  _nodeState.setLocked(locked);

  if (_nodeState.isRoot())
  {
    return;
  }

  setFlag(QGraphicsItem::ItemIsFocusable, !locked);
  setFlag(QGraphicsItem::ItemIsMovable, !locked);
  setFlag(QGraphicsItem::ItemIsSelectable, !locked);
}

void
NodeGraphicsObject::
paint(QPainter*                       painter,
      QStyleOptionGraphicsItem const* option,
      QWidget*)
{
  painter->setClipRect(option->exposedRect);

  NodePainter::paint(painter, *this);
}


QVariant
NodeGraphicsObject::
itemChange(GraphicsItemChange change, const QVariant& value)
{
  if (change == ItemScenePositionHasChanged && scene())
  {
    moveConnections();
  }

  return QGraphicsObject::itemChange(change, value);
}


void
NodeGraphicsObject::
mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  if (_nodeState.locked())
    return;

  for (PortType portToCheck: {PortType::In, PortType::Out})
  {
    NodeGeometry nodeGeometry(*this);

    PortIndex const portIndex =
      nodeGeometry.checkHitScenePoint(portToCheck,
                                      event->scenePos(),
                                      sceneTransform());

    if (portIndex != InvalidPortIndex)
    {
      auto const& connected =
        _graphModel.connections(_nodeId, portToCheck, portIndex);

      // Start dragging existing connection.
      if (!connected.empty() && portToCheck == PortType::In)
      {
        auto const& cnId = *connected.begin();

        // Need ConnectionGraphicsObject

        NodeConnectionInteraction
          interaction(*this,
                      *nodeScene()->connectionGraphicsObject(cnId),
                      *nodeScene());

        interaction.disconnect(portToCheck);
      }
      else // initialize new Connection
      {
        if (portToCheck == PortType::Out)
        {
          auto const outPolicy =
            _graphModel.portData(_nodeId,
                                 portToCheck,
                                 portIndex,
                                 PortRole::ConnectionPolicyRole).value<ConnectionPolicy>();

          if (!connected.empty() &&
              outPolicy == ConnectionPolicy::One)
          {
            for (auto& cnId : connected)
            {
              _graphModel.deleteConnection(cnId);
              Q_EMIT nodeScene()->connectionRemoved();
            }
          }

          ConnectionId const incompleteConnectionId =
             makeIncompleteConnectionId(_nodeId,
                                        portToCheck,
                                        portIndex);

          nodeScene()->makeDraftConnection(incompleteConnectionId);

          if (_nodeState.isRoot())   // !flags().testFlag(ItemIsSelectable)
             return;
        } // if port == out

      }
    }
  }

  if (_graphModel.nodeFlags(_nodeId) & NodeFlag::Resizable)
  {
    NodeGeometry geometry(*this);

    auto pos = event->pos();
    bool const hit = geometry.resizeRect().contains(QPoint(pos.x(), pos.y()));
    _nodeState.setResizing(hit);
  }

  auto otherButtons = event->buttons() ^ event->button();
  if (otherButtons == Qt::NoButton)
  {
    _nodeState.setPressedPos(event->scenePos());
  }

  QGraphicsObject::mousePressEvent(event);

  if (isSelected())
  {
    Q_EMIT nodeScene()->nodeSelected(_nodeId);
  }
}


void
NodeGraphicsObject::
mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  if (_nodeState.locked())
    return;

  // deselect all other items after this one is selected
  if (!isSelected())
  {
    scene()->clearSelection();
    setSelected(true);
  }

  if (_nodeState.resizing())
  {
    auto diff = event->pos() - event->lastPos();

    if (auto w = _graphModel.nodeData(_nodeId,
                                      NodeRole::Widget).value<QWidget*>())
    {
      prepareGeometryChange();

      auto oldSize = w->size();

      oldSize += QSize(diff.x(), diff.y());

      w->setFixedSize(oldSize);

      NodeGeometry geometry(*this);

      _proxyWidget->setMinimumSize(oldSize);
      _proxyWidget->setMaximumSize(oldSize);
      _proxyWidget->setPos(geometry.widgetPosition());

      // Passes the new size to the model.
      geometry.recalculateSize();

      update();

      moveConnections();

      event->accept();
    }
  }
  else
  {
    auto diff = event->pos() - event->lastPos();

    nodeScene()->undoStack().push(new MoveNodeCommand(nodeScene(),
                                                      _nodeId,
                                                      diff));

    event->accept();
  }

  QRectF r = scene()->sceneRect();

  r = r.united(mapToScene(boundingRect()).boundingRect());

  scene()->setSceneRect(r);
}


void
NodeGraphicsObject::
mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  if (_nodeState.locked())
    return;

  if (!_nodeState.resizing() && _nodeState.pressedPos() != event->scenePos())
  {
    Q_EMIT nodeScene()->nodeMoved(_nodeId, scenePos());

    auto otherButtons = event->buttons();
    if (otherButtons == Qt::NoButton)
    {
      _nodeState.setPressedPos(event->scenePos());
    }
  }
  _nodeState.setResizing(false);

  QGraphicsObject::mouseReleaseEvent(event);

  // position connections precisely after fast node move
  moveConnections();

  nodeScene()->nodeClicked(_nodeId);
}


void
NodeGraphicsObject::
hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
  // bring all the colliding nodes to background
  QList<QGraphicsItem*> overlapItems = collidingItems();

  for (QGraphicsItem* item : overlapItems)
  {
    if (item->zValue() > 0.0)
    {
      item->setZValue(0.0);
    }
  }

  // bring this node forward
  setZValue(1.0);

  _nodeState.setHovered(true);

  update();

  Q_EMIT nodeScene()->nodeHovered(_nodeId, event->screenPos());

  event->accept();
}


void
NodeGraphicsObject::
hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
  _nodeState.setHovered(false);

  update();

  Q_EMIT nodeScene()->nodeHoverLeft(_nodeId);

  event->accept();
}


void
NodeGraphicsObject::
hoverMoveEvent(QGraphicsSceneHoverEvent* event)
{
  auto pos = event->pos();

  NodeGeometry geometry(*this);

  if ((_graphModel.nodeFlags(_nodeId) | NodeFlag::Resizable) &&
      geometry.resizeRect().contains(QPoint(pos.x(), pos.y())))
  {
    setCursor(QCursor(Qt::SizeFDiagCursor));
  }
  else
  {
    setCursor(QCursor());
  }

  event->accept();
}


void
NodeGraphicsObject::
mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
  QGraphicsItem::mouseDoubleClickEvent(event);

  Q_EMIT nodeScene()->nodeDoubleClicked(_nodeId);
}


void
NodeGraphicsObject::
contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
  if (_nodeState.locked() || _nodeState.isRoot())
    return;

  Q_EMIT nodeScene()->nodeContextMenu(_nodeId, mapToScene(event->pos()));
}

}   // namespace QtNodes
