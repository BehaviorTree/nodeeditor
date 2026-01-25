#pragma once

#include "Export.hpp"

#include <QPainter>
#include <QPainterPath>

namespace QtNodes {

class ConnectionGraphicsObject;

/// Abstract base class for connection painters
class NODE_EDITOR_PUBLIC AbstractConnectionPainter
{
public:
    virtual ~AbstractConnectionPainter() = default;

    /// Paint the connection
    virtual void paint(QPainter *painter, ConnectionGraphicsObject const &cgo) const = 0;

    /// Get the painter stroke path for hit testing
    virtual QPainterPath getPainterStroke(ConnectionGraphicsObject const &cgo) const = 0;
};

} // namespace QtNodes
