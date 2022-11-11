//---------------------------------
// Transform Dialog
// Transform Tool ÇÃÉIÉvÉVÉáÉìÇëÄçÏÇ∑ÇÈ
//---------------------------------
#ifndef TRANSFORMDIALOG_H
#define TRANSFORMDIALOG_H

#include "iwdialog.h"

class QCheckBox;

class TransformDialog : public IwDialog {
  Q_OBJECT

  QCheckBox* m_scaleAroundCenterCB;
  QCheckBox* m_rotateAroundCenterCB;
  QCheckBox* m_shapeIndependentTransformsCB;
  // QCheckBox* m_frameIndependentTransformsCB;
  QCheckBox* m_translateOnlyCB;

public:
  TransformDialog();

protected slots:
  void onCheckBoxChanged();
};

#endif