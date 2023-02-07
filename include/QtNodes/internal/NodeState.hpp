#pragma once

#include <unordered_map>
#include <vector>

#include <QtCore/QPointF>
#include <QtCore/QPointer>
#include <QtCore/QUuid>

#include "Export.hpp"

#include "Definitions.hpp"
#include "NodeData.hpp"

namespace QtNodes {

class ConnectionGraphicsObject;
class NodeGraphicsObject;

/// Stores bool for hovering connections and resizing flag.
class NODE_EDITOR_PUBLIC NodeState
{
public:
    NodeState(NodeGraphicsObject &ngo);

public:
    bool hovered() const { return _hovered; }

    void setHovered(bool hovered = true) { _hovered = hovered; }

    void setResizing(bool resizing);

    bool locked() const { return _locked; }

    void setLocked(bool locked = true) { _locked = locked; }

    bool isRoot() const { return _root; }

    void setRoot(bool root = true) { _root = root; }

    bool resizing() const;

    ConnectionGraphicsObject const *connectionForReaction() const;

    void storeConnectionForReaction(ConnectionGraphicsObject const *cgo);

    void resetConnectionForReaction();

    QPointF pressedPos() const;
    void setPressedPos(QPointF newPressedPos);

private:
    NodeGraphicsObject &_ngo;

    bool _hovered;

    bool _locked;

    bool _root;

    bool _resizing;

    QPointF _pressedPos;

    // QPointer tracks the QObject inside and is automatically cleared
    // when the object is destroyed.
    QPointer<ConnectionGraphicsObject const> _connectionForReaction;
};
} // namespace QtNodes
