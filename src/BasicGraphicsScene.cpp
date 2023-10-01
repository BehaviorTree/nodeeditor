#include "BasicGraphicsScene.hpp"

#include "AbstractNodeGeometry.hpp"
#include "ConnectionGraphicsObject.hpp"
#include "ConnectionIdUtils.hpp"
#include "DefaultHorizontalNodeGeometry.hpp"
#include "DefaultNodePainter.hpp"
#include "DefaultVerticalNodeGeometry.hpp"
#include "GraphicsView.hpp"
#include "NodeGraphicsObject.hpp"

#include <QUndoStack>

#include <QtWidgets/QFileDialog>
#include <QtWidgets/QGraphicsSceneMoveEvent>

#include <QtCore/QBuffer>
#include <QtCore/QByteArray>
#include <QtCore/QDataStream>
#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QtGlobal>

#include <iostream>
#include <stdexcept>
#include <unordered_set>
#include <utility>
#include <queue>

namespace QtNodes {

BasicGraphicsScene::BasicGraphicsScene(AbstractGraphModel &graphModel, QObject *parent)
    : QGraphicsScene(parent)
    , _graphModel(graphModel)
    , _nodeGeometry(std::make_unique<DefaultHorizontalNodeGeometry>(_graphModel))
    , _nodePainter(std::make_unique<DefaultNodePainter>())
    , _nodeDrag(false)
    , _undoStack(new QUndoStack(this))
    , _orientation(Qt::Horizontal)
{
    setItemIndexMethod(QGraphicsScene::NoIndex);

    connect(&_graphModel,
            &AbstractGraphModel::connectionCreated,
            this,
            &BasicGraphicsScene::onConnectionCreated);

    connect(&_graphModel,
            &AbstractGraphModel::connectionDeleted,
            this,
            &BasicGraphicsScene::onConnectionDeleted);

    connect(&_graphModel,
            &AbstractGraphModel::nodeCreated,
            this,
            &BasicGraphicsScene::onNodeCreated);

    connect(&_graphModel,
            &AbstractGraphModel::nodeDeleted,
            this,
            &BasicGraphicsScene::onNodeDeleted);

    connect(&_graphModel,
            &AbstractGraphModel::nodePositionUpdated,
            this,
            &BasicGraphicsScene::onNodePositionUpdated);

    connect(&_graphModel,
            &AbstractGraphModel::nodeUpdated,
            this,
            &BasicGraphicsScene::onNodeUpdated);

    connect(&_graphModel,
            &AbstractGraphModel::portLayoutUpdated,
            this,
            &BasicGraphicsScene::onPortLayoutUpdated);

    connect(&_graphModel,
            &AbstractGraphModel::styleUpdated,
            this,
            &BasicGraphicsScene::onStyleUpdated);

    connect(&_graphModel,
            &AbstractGraphModel::nodeFlagsUpdated,
            this,
            &BasicGraphicsScene::onFlagsUpdated);

    connect(this, &BasicGraphicsScene::nodeClicked, this, &BasicGraphicsScene::onNodeClicked);

    connect(&_graphModel, &AbstractGraphModel::modelReset, this, &BasicGraphicsScene::onModelReset);

    traverseGraphAndPopulateGraphicsObjects();
}

BasicGraphicsScene::~BasicGraphicsScene() = default;

AbstractGraphModel const &BasicGraphicsScene::graphModel() const
{
    return _graphModel;
}

AbstractGraphModel &BasicGraphicsScene::graphModel()
{
    return _graphModel;
}

AbstractNodeGeometry &BasicGraphicsScene::nodeGeometry()
{
    return *_nodeGeometry;
}

AbstractNodePainter &BasicGraphicsScene::nodePainter()
{
    return *_nodePainter;
}

void BasicGraphicsScene::setNodePainter(std::unique_ptr<AbstractNodePainter> newPainter)
{
    _nodePainter = std::move(newPainter);
}

QUndoStack &BasicGraphicsScene::undoStack()
{
    return *_undoStack;
}

std::unique_ptr<ConnectionGraphicsObject> const &BasicGraphicsScene::makeDraftConnection(
    ConnectionId const incompleteConnectionId)
{
    _draftConnection = std::make_unique<ConnectionGraphicsObject>(*this, incompleteConnectionId);

    _draftConnection->grabMouse();

    return _draftConnection;
}

void BasicGraphicsScene::resetDraftConnection(bool connected)
{
    if (!connected && _draftConnection) {
        auto dc = _draftConnection->connectionId();
        auto point = _draftConnection->pos() + _draftConnection->endPoint(PortType::In);
        Q_EMIT nodeInsertRequested(point, dc.outNodeId);
    }
    _draftConnection.reset();
}

void BasicGraphicsScene::clearScene()
{
    auto const &allNodeIds = graphModel().allNodeIds();

    for (auto nodeId : allNodeIds) {
        graphModel().deleteNode(nodeId);
    }
}

NodeGraphicsObject *BasicGraphicsScene::nodeGraphicsObject(NodeId nodeId)
{
    NodeGraphicsObject *ngo = nullptr;
    auto it = _nodeGraphicsObjects.find(nodeId);
    if (it != _nodeGraphicsObjects.end()) {
        ngo = it->second.get();
    }

    return ngo;
}

ConnectionGraphicsObject *BasicGraphicsScene::connectionGraphicsObject(ConnectionId connectionId)
{
    ConnectionGraphicsObject *cgo = nullptr;
    auto it = _connectionGraphicsObjects.find(connectionId);
    if (it != _connectionGraphicsObjects.end()) {
        cgo = it->second.get();
    }

    return cgo;
}

void BasicGraphicsScene::setOrientation(Qt::Orientation const orientation)
{
    if (_orientation != orientation) {
        _orientation = orientation;

        switch (_orientation) {
        case Qt::Horizontal:
            _nodeGeometry = std::make_unique<DefaultHorizontalNodeGeometry>(_graphModel);
            break;

        case Qt::Vertical:
            _nodeGeometry = std::make_unique<DefaultVerticalNodeGeometry>(_graphModel);
            break;
        }

        // having some trouble with reset! we just update! after calling this
        //        onModelReset();
    }
}

QMenu *BasicGraphicsScene::createSceneMenu(QPointF const scenePos)
{
    Q_UNUSED(scenePos);
    return nullptr;
}

std::vector<NodeId> BasicGraphicsScene::selectedNodes() const
{
    QList<QGraphicsItem *> graphicsItems = selectedItems();

    std::vector<NodeId> result;
    result.reserve(graphicsItems.size());

    for (QGraphicsItem *item : graphicsItems) {
        if (auto ngo = qgraphicsitem_cast<NodeGraphicsObject *>(item)) {
            result.push_back(ngo->nodeId());
        }
    }
    return result;
}

void BasicGraphicsScene::cleanupSceneMenu(QMenu *menu)
{
    Q_UNUSED(menu);
}

void BasicGraphicsScene::traverseGraphAndPopulateGraphicsObjects()
{
    auto allNodeIds = _graphModel.allNodeIds();

    // First create all the nodes.
    for (NodeId const nodeId : allNodeIds) {
        auto caption = _graphModel.nodeData(nodeId, NodeRole::Caption).toString();
        if (caption != "Root")
            _nodeGraphicsObjects[nodeId] = std::make_unique<NodeGraphicsObject>(*this, nodeId);
        else
            _nodeGraphicsObjects[nodeId] = std::make_unique<RootNodeObject>(*this, nodeId);
    }

    // Then for each node check output connections and insert them.
    for (NodeId const nodeId : allNodeIds) {
        unsigned int nOutPorts = _graphModel.nodeData<PortCount>(nodeId, NodeRole::OutPortCount);

        for (PortIndex index = 0; index < nOutPorts; ++index) {
            auto const &outConnectionIds = _graphModel.connections(nodeId, PortType::Out, index);

            for (auto cid : outConnectionIds) {
                _connectionGraphicsObjects[cid] = std::make_unique<ConnectionGraphicsObject>(*this,
                                                                                             cid);
            }
        }
    }
}

void BasicGraphicsScene::updateAllNodes()
{
    for (auto &[nodeId, nodes] : _nodeGraphicsObjects) {
        onNodeUpdated(nodeId);
    }
}

void BasicGraphicsScene::updateAttachedNodes(ConnectionId const connectionId,
                                             PortType const portType)
{
    auto node = nodeGraphicsObject(getNodeId(portType, connectionId));

    if (node) {
        node->update();
    }
}

void BasicGraphicsScene::onConnectionDeleted(ConnectionId const connectionId)
{
    auto it = _connectionGraphicsObjects.find(connectionId);
    if (it != _connectionGraphicsObjects.end()) {
        _connectionGraphicsObjects.erase(it);
    }

    // TODO: do we need it?
    if (_draftConnection && _draftConnection->connectionId() == connectionId) {
        _draftConnection.reset();
    }

    updateAttachedNodes(connectionId, PortType::Out);
    updateAttachedNodes(connectionId, PortType::In);
}

void BasicGraphicsScene::onConnectionCreated(ConnectionId const connectionId)
{
    _connectionGraphicsObjects[connectionId]
        = std::make_unique<ConnectionGraphicsObject>(*this, connectionId);

    updateAttachedNodes(connectionId, PortType::Out);
    updateAttachedNodes(connectionId, PortType::In);
}

void BasicGraphicsScene::onNodeDeleted(NodeId const nodeId)
{
    auto it = _nodeGraphicsObjects.find(nodeId);
    if (it != _nodeGraphicsObjects.end()) {
        _nodeGraphicsObjects.erase(it);
    }
}

void BasicGraphicsScene::onNodeCreated(NodeId const nodeId)
{
    auto caption = _graphModel.nodeData(nodeId, NodeRole::Caption).toString();
    if (caption != "Root")
        _nodeGraphicsObjects[nodeId] = std::make_unique<NodeGraphicsObject>(*this, nodeId);
    else
        _nodeGraphicsObjects[nodeId] = std::make_unique<RootNodeObject>(*this, nodeId);
}

void BasicGraphicsScene::onNodePositionUpdated(NodeId const nodeId)
{
    auto node = nodeGraphicsObject(nodeId);
    if (node) {
        node->setPos(_graphModel.nodeData(nodeId, NodeRole::Position).value<QPointF>());
        node->update();
        _nodeDrag = false;
    }
}

void BasicGraphicsScene::onNodeUpdated(NodeId const nodeId)
{
    auto node = nodeGraphicsObject(nodeId);

    if (node) {
        // commented for performance
        // since we are not using shadow and opacity no need to be called
        // node->updateStyle();
        node->setGeometryChanged();

        _nodeGeometry->recomputeSize(nodeId);
        node->updateWidgetPosition();
        node->update();
        node->moveConnections();
    }
}

void BasicGraphicsScene::onNodeClicked(NodeId const nodeId)
{
    if (_nodeDrag)
        Q_EMIT nodeMoved(nodeId, _graphModel.nodeData(nodeId, NodeRole::Position).value<QPointF>());
    _nodeDrag = false;
}

void BasicGraphicsScene::onModelReset()
{
    _connectionGraphicsObjects.clear();
    _nodeGraphicsObjects.clear();

    clear();

    traverseGraphAndPopulateGraphicsObjects();
}

void BasicGraphicsScene::onPortLayoutUpdated(PortLayout layout)
{
    setOrientation(layout == QtNodes::PortLayout::Horizontal ? Qt::Horizontal : Qt::Vertical);
    updateAllNodes();
}

void BasicGraphicsScene::onStyleUpdated()
{
    updateAllNodes();
}

void BasicGraphicsScene::onFlagsUpdated(const NodeId nodeId)
{
    NodeFlags flags = _graphModel.nodeFlags(nodeId);
    bool const locked = flags.testFlag(NodeFlag::Locked);

    size_t const n = _graphModel.nodeData(nodeId, NodeRole::OutPortCount).toUInt();
    for (PortIndex portIndex = 0; portIndex < n; ++portIndex) {
        auto const &connected = _graphModel.connections(nodeId, PortType::Out, portIndex);

        for (auto &cnId : connected) {
            auto cgo = connectionGraphicsObject(cnId);

            if (cgo) {
                cgo->lock(locked);
                cgo->update();
            }
        }
    }
}

} // namespace QtNodes
