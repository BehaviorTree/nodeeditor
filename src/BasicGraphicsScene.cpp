#include "BasicGraphicsScene.hpp"

#include <queue>
#include <iostream>
#include <stdexcept>
#include <unordered_set>
#include <utility>

#include <QtGui/QUndoStack>

#include <QtWidgets/QGraphicsSceneMoveEvent>
#include <QtWidgets/QFileDialog>

#include <QtCore/QBuffer>
#include <QtCore/QByteArray>
#include <QtCore/QDataStream>
#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QtGlobal>

#include "ConnectionGraphicsObject.hpp"
#include "ConnectionIdUtils.hpp"
#include "GraphicsView.hpp"
#include "NodeGraphicsObject.hpp"


namespace QtNodes
{

BasicGraphicsScene::
BasicGraphicsScene(AbstractGraphModel &graphModel,
                   QObject *   parent)
  : QGraphicsScene(parent)
  , _graphModel(graphModel)
  , _undoStack(new QUndoStack(this))
{
  setItemIndexMethod(QGraphicsScene::NoIndex);


  connect(&_graphModel, &AbstractGraphModel::connectionCreated,
          this, &BasicGraphicsScene::onConnectionCreated);

  connect(&_graphModel, &AbstractGraphModel::connectionDeleted,
          this, &BasicGraphicsScene::onConnectionDeleted);

  connect(&_graphModel, &AbstractGraphModel::nodeCreated,
          this, &BasicGraphicsScene::onNodeCreated);

  connect(&_graphModel, &AbstractGraphModel::nodeDeleted,
          this, &BasicGraphicsScene::onNodeDeleted);

  connect(&_graphModel, &AbstractGraphModel::nodeResized,
          this, &BasicGraphicsScene::onNodeResized);

  connect(&_graphModel, &AbstractGraphModel::nodePositionUpdated,
          this, &BasicGraphicsScene::onNodePositionUpdated);

  connect(&_graphModel, &AbstractGraphModel::portsAboutToBeDeleted,
          this, &BasicGraphicsScene::onPortsAboutToBeDeleted);

  connect(&_graphModel, &AbstractGraphModel::portsDeleted,
          this, &BasicGraphicsScene::onPortsDeleted);

  connect(&_graphModel, &AbstractGraphModel::portsAboutToBeInserted,
          this, &BasicGraphicsScene::onPortsAboutToBeInserted);

  connect(&_graphModel, &AbstractGraphModel::portsInserted,
          this, &BasicGraphicsScene::onPortsInserted);

  traverseGraphAndPopulateGraphicsObjects();
}


BasicGraphicsScene::
~BasicGraphicsScene() = default;

AbstractGraphModel const &
BasicGraphicsScene::
graphModel() const
{
  return _graphModel;
}


AbstractGraphModel &
BasicGraphicsScene::
graphModel()
{
  return _graphModel;
}


QUndoStack &
BasicGraphicsScene::
undoStack()
{
  return *_undoStack;
}


std::unique_ptr<ConnectionGraphicsObject> const &
BasicGraphicsScene::
makeDraftConnection(ConnectionId const incompleteConnectionId)
{
  _draftConnection =
    std::make_unique<ConnectionGraphicsObject>(*this,
                                               incompleteConnectionId);

  _draftConnection->grabMouse();

  return _draftConnection;
}


void
BasicGraphicsScene::
resetDraftConnection()
{
  _draftConnection.reset();
}


void
BasicGraphicsScene::
clearScene()
{
  auto const &allNodeIds =
    graphModel().allNodeIds();

  for ( auto nodeId : allNodeIds)
  {
    graphModel().deleteNode(nodeId);
  }
}


void
BasicGraphicsScene::
lockNode(const NodeId nodeId, bool locked)
{
  auto node = nodeGraphicsObject(nodeId);
  if (node)
  {
    node->lock(locked);

    size_t const n = _graphModel.nodeData(nodeId, NodeRole::NumberOfOutPorts).toUInt();
    for (PortIndex portIndex = 0; portIndex < n; ++portIndex)
    {
      auto const& connected = _graphModel.connections(nodeId, PortType::Out, portIndex);

      for (auto& cnId : connected)
      {
        auto cgo = connectionGraphicsObject(cnId);

        if (cgo)
        {
          cgo->lock(locked);
          cgo->update();
        }
      }
    }

    node->update();
  }
}

NodeGraphicsObject*
BasicGraphicsScene::
nodeGraphicsObject(NodeId nodeId)
{
  NodeGraphicsObject * ngo = nullptr;
  auto it = _nodeGraphicsObjects.find(nodeId);
  if (it != _nodeGraphicsObjects.end())
  {
    ngo = it->second.get();
  }

  return ngo;
}


ConnectionGraphicsObject*
BasicGraphicsScene::
connectionGraphicsObject(ConnectionId connectionId)
{
  ConnectionGraphicsObject * cgo = nullptr;
  auto it = _connectionGraphicsObjects.find(connectionId);
  if (it != _connectionGraphicsObjects.end())
  {
    cgo = it->second.get();
  }

  return cgo;
}


QMenu *
BasicGraphicsScene::
createSceneMenu(QPointF const scenePos)
{
  Q_UNUSED(scenePos);
  return nullptr;
}

std::vector<NodeId> BasicGraphicsScene::selectedNodes() const
{
  QList<QGraphicsItem*> graphicsItems = selectedItems();

  std::vector<NodeId> result;
  result.reserve(graphicsItems.size());

  for (QGraphicsItem* item : graphicsItems)
  {
    if (auto ngo = qgraphicsitem_cast<NodeGraphicsObject*>(item))
    {
      result.push_back(ngo->nodeId());
    }
  }
  return result;
}

void
BasicGraphicsScene::
cleanupSceneMenu(QMenu* menu)
{
  Q_UNUSED(menu);
}

void
BasicGraphicsScene::
traverseGraphAndPopulateGraphicsObjects()
{
  auto allNodeIds = _graphModel.allNodeIds();

  std::vector<ConnectionId> connectionsToCreate;

  while (!allNodeIds.empty())
  {
    std::queue<NodeId> fifo;

    auto firstId = *allNodeIds.begin();
    allNodeIds.erase(firstId);

    fifo.push(firstId);

    while (!fifo.empty())
    {
      auto nodeId = fifo.front();
      fifo.pop();

      auto caption = _graphModel.nodeData(nodeId, NodeRole::Caption).toString();
      if ( caption != "Root")
          _nodeGraphicsObjects[nodeId] =
            std::make_unique<NodeGraphicsObject>(*this, nodeId);
      else
          _nodeGraphicsObjects[nodeId] =
            std::make_unique<RootNodeObject>(*this, nodeId);

      unsigned int nOutPorts =
        _graphModel.nodeData(nodeId, NodeRole::NumberOfOutPorts).toUInt();

      for (PortIndex index = 0; index < nOutPorts; ++index)
      {
        auto const& conns =
          _graphModel.connections(nodeId,
                                  PortType::Out,
                                  index);

        for (auto cn : conns)
        {
          fifo.push(cn.inNodeId);
          allNodeIds.erase(cn.inNodeId);

          connectionsToCreate.push_back(cn);
        }
      }
    } // while
  }

  for (auto const & connectionId : connectionsToCreate)
  {
    _connectionGraphicsObjects[connectionId] =
      std::make_unique<ConnectionGraphicsObject>(*this,
                                                 connectionId);
  }
}


void
BasicGraphicsScene::
updateAttachedNodes(ConnectionId const connectionId,
                    PortType const portType)
{
  auto node =
    nodeGraphicsObject(getNodeId(portType, connectionId));

  if (node)
  {
    node->update();
  }
}


void
BasicGraphicsScene::
onConnectionDeleted(ConnectionId const connectionId)
{
  auto it = _connectionGraphicsObjects.find(connectionId);
  if (it != _connectionGraphicsObjects.end())
  {
    _connectionGraphicsObjects.erase(it);
  }

  // TODO: do we need it?
  if (_draftConnection &&
      _draftConnection->connectionId() == connectionId)
  {
    _draftConnection.reset();
  }

  updateAttachedNodes(connectionId, PortType::Out);
  updateAttachedNodes(connectionId, PortType::In);
}


void
BasicGraphicsScene::
onConnectionCreated(ConnectionId const connectionId)
{
  _connectionGraphicsObjects[connectionId] =
    std::make_unique<ConnectionGraphicsObject>(*this,
                                               connectionId);

  updateAttachedNodes(connectionId, PortType::Out);
  updateAttachedNodes(connectionId, PortType::In);
}


void
BasicGraphicsScene::
onNodeDeleted(NodeId const nodeId)
{
  auto it = _nodeGraphicsObjects.find(nodeId);
  if (it != _nodeGraphicsObjects.end())
  {
    _nodeGraphicsObjects.erase(it);
  }
}

void
BasicGraphicsScene::
onNodeResized(const NodeId nodeId)
{
  auto it = _nodeGraphicsObjects.find(nodeId);
  if (it != _nodeGraphicsObjects.end())
  {
    it->second->onNodeResized();
  }
}

void
BasicGraphicsScene::
onNodeCreated(NodeId const nodeId)
{
    auto caption = _graphModel.nodeData(nodeId, NodeRole::Caption).toString();
    if ( caption != "Root")
        _nodeGraphicsObjects[nodeId] =
          std::make_unique<NodeGraphicsObject>(*this, nodeId);
    else
        _nodeGraphicsObjects[nodeId] =
          std::make_unique<RootNodeObject>(*this, nodeId);
}


void
BasicGraphicsScene::
onNodePositionUpdated(NodeId const nodeId)
{
  auto node = nodeGraphicsObject(nodeId);
  if (node)
  {
    node->setPos(_graphModel.nodeData(nodeId,
                                      NodeRole::Position).value<QPointF>());
    node->update();
  }
}


void
BasicGraphicsScene::
onPortsAboutToBeDeleted(NodeId const nodeId,
                        PortType const portType,
                        std::unordered_set<PortIndex> const &portIndexSet)
{
  Q_UNUSED(nodeId);
  Q_UNUSED(portType);
  Q_UNUSED(portIndexSet);
  //NodeGraphicsObject * node = nodeGraphicsObject(nodeId);

  //if (node)
  //{
  //for (auto portIndex : portIndexSet)
  //{
  //auto const connectedNodes =
  //_graphModel.connectedNodes(nodeId, portType, portIndex);

  //for (auto cn : connectedNodes)
  //{
  //ConnectionId connectionId =
  //(portType == PortType::In) ?
  //std::make_tuple(cn.first, cn.second, nodeId, portIndex) :
  //std::make_tuple(nodeId, portIndex, cn.first, cn.second);

  //deleteConnection(connectionId);
  //}
  //}
  //}
}


void
BasicGraphicsScene::
onPortsDeleted(NodeId const nodeId,
               PortType const portType,
               std::unordered_set<PortIndex> const &portIndexSet)
{
  Q_UNUSED(nodeId);
  Q_UNUSED(portType);
  Q_UNUSED(portIndexSet);

  NodeGraphicsObject * node = nodeGraphicsObject(nodeId);

  if (node)
  {
    node->update();
  }
}


void
BasicGraphicsScene::
onPortsAboutToBeInserted(NodeId const nodeId,
                         PortType const portType,
                         std::unordered_set<PortIndex> const &portIndexSet)
{
  Q_UNUSED(nodeId);
  Q_UNUSED(portType);
  Q_UNUSED(portIndexSet);
}


void
BasicGraphicsScene::
onPortsInserted(NodeId const nodeId,
                PortType const portType,
                std::unordered_set<PortIndex> const &portIndexSet)
{
  Q_UNUSED(nodeId);
  Q_UNUSED(portType);
  Q_UNUSED(portIndexSet);
}

void BasicGraphicsScene::onPortLayoutUpdated(PortLayout)
{
  for(auto& [nodeId, nodes]: _nodeGraphicsObjects)
  {
    nodes->moveConnections();
    nodes->update();
  }
}

void BasicGraphicsScene::onNodeColorUpdated(const NodeId nodeId)
{
  // to update the inner connection when status is changed
  auto const& connected = graphModel().connections(nodeId, PortType::In, 0);
  for (auto& cnId : connected)
  {
    auto cgo = connectionGraphicsObject(cnId);
    if (cgo)
    {
      cgo->update();
    }
  }
}
}
