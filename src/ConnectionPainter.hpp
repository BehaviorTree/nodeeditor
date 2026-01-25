#pragma once

#include <QtGui/QPainter>
#include <QtGui/QPainterPath>

#include "Definitions.hpp"
#include "AbstractConnectionPainter.hpp"

namespace QtNodes {

class ConnectionGeometry;
class ConnectionGraphicsObject;

/// Default connection painter implementation
class DefaultConnectionPainter : public AbstractConnectionPainter
{
public:
    void paint(QPainter *painter, ConnectionGraphicsObject const &cgo) const override;

    QPainterPath getPainterStroke(ConnectionGraphicsObject const &cgo) const override;
};

// Backwards compatibility alias
using ConnectionPainter = DefaultConnectionPainter;

} // namespace QtNodes
