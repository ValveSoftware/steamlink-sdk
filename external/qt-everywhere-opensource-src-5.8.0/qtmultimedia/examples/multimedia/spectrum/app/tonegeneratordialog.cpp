/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "tonegeneratordialog.h"
#include <QComboBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QSlider>
#include <QSpinBox>

const int ToneGeneratorFreqMin = 1;
const int ToneGeneratorFreqMax = 1000;
const int ToneGeneratorFreqDefault = 440;
const int ToneGeneratorAmplitudeDefault = 75;

ToneGeneratorDialog::ToneGeneratorDialog(QWidget *parent)
    :   QDialog(parent)
    ,   m_toneGeneratorSweepCheckBox(new QCheckBox(tr("Frequency sweep"), this))
    ,   m_frequencySweepEnabled(true)
    ,   m_toneGeneratorControl(new QWidget(this))
    ,   m_toneGeneratorFrequencyControl(new QWidget(this))
    ,   m_frequencySlider(new QSlider(Qt::Horizontal, this))
    ,   m_frequencySpinBox(new QSpinBox(this))
    ,   m_frequency(ToneGeneratorFreqDefault)
    ,   m_amplitudeSlider(new QSlider(Qt::Horizontal, this))
{
    QVBoxLayout *dialogLayout = new QVBoxLayout(this);

    m_toneGeneratorSweepCheckBox->setChecked(true);

    // Configure tone generator controls
    m_frequencySlider->setRange(ToneGeneratorFreqMin, ToneGeneratorFreqMax);
    m_frequencySlider->setValue(ToneGeneratorFreqDefault);
    m_frequencySpinBox->setRange(ToneGeneratorFreqMin, ToneGeneratorFreqMax);
    m_frequencySpinBox->setValue(ToneGeneratorFreqDefault);
    m_amplitudeSlider->setRange(0, 100);
    m_amplitudeSlider->setValue(ToneGeneratorAmplitudeDefault);

    // Add widgets to layout
    QGridLayout *frequencyControlLayout = new QGridLayout;
    QLabel *frequencyLabel = new QLabel(tr("Frequency (Hz)"), this);
    frequencyControlLayout->addWidget(frequencyLabel, 0, 0, 2, 1);
    frequencyControlLayout->addWidget(m_frequencySlider, 0, 1);
    frequencyControlLayout->addWidget(m_frequencySpinBox, 1, 1);
    m_toneGeneratorFrequencyControl->setLayout(frequencyControlLayout);
    m_toneGeneratorFrequencyControl->setEnabled(false);

    QGridLayout *toneGeneratorLayout = new QGridLayout;
    QLabel *amplitudeLabel = new QLabel(tr("Amplitude"), this);
    toneGeneratorLayout->addWidget(m_toneGeneratorSweepCheckBox, 0, 1);
    toneGeneratorLayout->addWidget(m_toneGeneratorFrequencyControl, 1, 0, 1, 2);
    toneGeneratorLayout->addWidget(amplitudeLabel, 2, 0);
    toneGeneratorLayout->addWidget(m_amplitudeSlider, 2, 1);
    m_toneGeneratorControl->setLayout(toneGeneratorLayout);
    m_toneGeneratorControl->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    dialogLayout->addWidget(m_toneGeneratorControl);

    // Connect
    CHECKED_CONNECT(m_toneGeneratorSweepCheckBox, SIGNAL(toggled(bool)),
                    this, SLOT(frequencySweepEnabled(bool)));
    CHECKED_CONNECT(m_frequencySlider, SIGNAL(valueChanged(int)),
                    m_frequencySpinBox, SLOT(setValue(int)));
    CHECKED_CONNECT(m_frequencySpinBox, SIGNAL(valueChanged(int)),
                    m_frequencySlider, SLOT(setValue(int)));

    // Add standard buttons to layout
    QDialogButtonBox *buttonBox = new QDialogButtonBox(this);
    buttonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    dialogLayout->addWidget(buttonBox);

    // Connect standard buttons
    CHECKED_CONNECT(buttonBox->button(QDialogButtonBox::Ok), SIGNAL(clicked()),
                    this, SLOT(accept()));
    CHECKED_CONNECT(buttonBox->button(QDialogButtonBox::Cancel), SIGNAL(clicked()),
                    this, SLOT(reject()));

    setLayout(dialogLayout);
}

ToneGeneratorDialog::~ToneGeneratorDialog()
{

}

bool ToneGeneratorDialog::isFrequencySweepEnabled() const
{
    return m_toneGeneratorSweepCheckBox->isChecked();
}

qreal ToneGeneratorDialog::frequency() const
{
    return qreal(m_frequencySlider->value());
}

qreal ToneGeneratorDialog::amplitude() const
{
    return qreal(m_amplitudeSlider->value()) / 100.0;
}

void ToneGeneratorDialog::frequencySweepEnabled(bool enabled)
{
    m_frequencySweepEnabled = enabled;
    m_toneGeneratorFrequencyControl->setEnabled(!enabled);
}
