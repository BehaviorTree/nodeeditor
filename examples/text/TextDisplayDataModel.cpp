#include "TextDisplayDataModel.hpp"

TextDisplayDataModel::
TextDisplayDataModel()
  : _label(new QLabel("Resulting Text"))
{
  _label->setMargin(3);
}


unsigned int
TextDisplayDataModel::
nPorts(PortType portType) const
{
  unsigned int result = 1;

  switch (portType)
  {
    case PortType::In:
      result = 1;
      break;

    case PortType::Out:
      result = 0;

    default:
      break;
  }

  return result;
}


NodeDataType
TextDisplayDataModel::
dataType(PortType, PortIndex) const
{
  return TextData().type();
}


std::shared_ptr<NodeData>
TextDisplayDataModel::
outData(PortIndex)
{
  std::shared_ptr<NodeData> ptr;
  return ptr;
}


void
TextDisplayDataModel::
setInData(std::shared_ptr<NodeData> data, 
          PortIndex const)
{
  auto textData = std::dynamic_pointer_cast<TextData>(data);

  if (textData)
  {
    _inputText = textData->text();
  }
  else
  {
    _inputText = "";
  }

  _label->setText(_inputText);
  _label->adjustSize();
}

