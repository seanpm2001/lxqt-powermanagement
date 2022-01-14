/*
* Copyright (c) 2014, 2015 Christian Surlykke, Paulo Lieuthier
*
* This file is part of the LXQt project. <https://lxqt.org>
* It is distributed under the LGPL 2.1 or later license.
* Please refer to the LICENSE file for a copy of the license.
*/
#include "iconproducer.h"

#include <LXQt/Globals>
#include <LXQt/Settings>
#include <XdgIcon>
#include <QDebug>
#include <QtSvg/QSvgRenderer>
#include <QPainter>
#include <cmath>

IconProducer::IconProducer(Solid::Battery *battery, QObject *parent) : QObject(parent)
{
    connect(battery, &Solid::Battery::chargeStateChanged, this, &IconProducer::updateState);
    connect(battery, &Solid::Battery::chargePercentChanged, this, &IconProducer::updateChargePercent);
    connect(&mSettings, &PowerManagementSettings::settingsChanged, this, &IconProducer::update);

    mChargePercent = battery->chargePercent();
    mState = battery->chargeState();
    themeChanged();
}

IconProducer::IconProducer(QObject *parent):  QObject(parent)
{
    themeChanged();
    update();
}


void IconProducer::updateChargePercent(int newChargePercent)
{
    mChargePercent = newChargePercent;

    update();
}

void IconProducer::updateState(int newState)
{
    mState = (Solid::Battery::ChargeState) newState;

    update();
}

void IconProducer::update()
{
    QString newIconName;

    if (mSettings.isUseThemeIcons())
    {
        QMap<float, QString> *levelNameMap = (mState == Solid::Battery::Discharging ? &mLevelNameMapDischarging : &mLevelNameMapCharging);
        const auto levels = levelNameMap->keys();
        for (const float level : levels)
        {
            if (level >= mChargePercent)
            {
                newIconName = levelNameMap->value(level);
                break;
            }
        }
    }

    if (mSettings.isUseThemeIcons() && newIconName == mIconName)
        return;

    mIconName = newIconName;
    mIcon = mSettings.isUseThemeIcons() ? QIcon::fromTheme(mIconName) : circleIcon();
    emit iconChanged();
}

void IconProducer::themeChanged()
{
    /*
     * We maintain specific charge-level-to-icon-name mappings for Oxygen and Awoken and
     * asume all other themes adhere to the freedesktop standard.
     * This is certainly not true, and as bug reports come in stating that
     * this and that theme is not working we will probably have to add new
     * mappings beside Oxygen and Awoken
     */
    mLevelNameMapDischarging.clear();
    mLevelNameMapCharging.clear();

    if (QIcon::themeName() == QL1S("oxygen"))
    {
        // Means:
        // Use 'battery-low' for levels up to 10
        //  -  'battery-caution' for levels between 10 and 20
        //  -  'battery-040' for levels between 20 and 40, etc..
        mLevelNameMapDischarging[10] = QL1S("battery-low");
        mLevelNameMapDischarging[20] = QL1S("battery-caution");
        mLevelNameMapDischarging[40] = QL1S("battery-040");
        mLevelNameMapDischarging[60] = QL1S("battery-060");
        mLevelNameMapDischarging[80] = QL1S("battery-080");
        mLevelNameMapDischarging[101] = QL1S("battery-100");
        mLevelNameMapCharging[10] = QL1S("battery-charging-low");
        mLevelNameMapCharging[20] = QL1S("battery-charging-caution");
        mLevelNameMapCharging[40] = QL1S("battery-charging-040");
        mLevelNameMapCharging[60] = QL1S("battery-charging-060");
        mLevelNameMapCharging[80] = QL1S("battery-charging-080");
        mLevelNameMapCharging[101] = QL1S("battery-charging");
    }
    else if (QIcon::themeName().startsWith(QL1S("AwOken"))) // AwOken, AwOkenWhite, AwOkenDark
    {
        mLevelNameMapDischarging[5] = QL1S("battery-000");
        mLevelNameMapDischarging[30] = QL1S("battery-020");
        mLevelNameMapDischarging[50] = QL1S("battery-040");
        mLevelNameMapDischarging[70] = QL1S("battery-060");
        mLevelNameMapDischarging[95] = QL1S("battery-080");
        mLevelNameMapDischarging[101] = QL1S("battery-100");
        mLevelNameMapCharging[5] = QL1S("battery-000-charging");
        mLevelNameMapCharging[30] = QL1S("battery-020-charging");
        mLevelNameMapCharging[50] = QL1S("battery-040-charging");
        mLevelNameMapCharging[70] = QL1S("battery-060-charging");
        mLevelNameMapCharging[95] = QL1S("battery-080-charging");
        mLevelNameMapCharging[101] = QL1S("battery-100-charging");
    }
    else // As default we fall back to the freedesktop scheme.
    {
        mLevelNameMapDischarging[3] = QL1S("battery-empty");
        mLevelNameMapDischarging[10] = QL1S("battery-caution");
        mLevelNameMapDischarging[50] = QL1S("battery-low");
        mLevelNameMapDischarging[90] = QL1S("battery-good");
        mLevelNameMapDischarging[101] = QL1S("battery-full");
        mLevelNameMapCharging[3] = QL1S("battery-empty");
        mLevelNameMapCharging[10] = QL1S("battery-caution-charging");
        mLevelNameMapCharging[50] = QL1S("battery-low-charging");
        mLevelNameMapCharging[90] = QL1S("battery-good-charging");
        mLevelNameMapCharging[101] = QL1S("battery-full-charging");
    }

    update();
}

QIcon& IconProducer::circleIcon()
{
    static QMap<Solid::Battery::ChargeState, QMap<int, QIcon> > cache;

    if (!cache[mState].contains(mChargePercent))
        cache[mState][mChargePercent] = buildCircleIcon(mState, mChargePercent);

    return cache[mState][mChargePercent];
}

QIcon IconProducer::buildCircleIcon(Solid::Battery::ChargeState state, int chargeLevel)
{
    static QString svg_template = QL1S(
        "<svg\n"
        "    xmlns='http://www.w3.org/2000/svg'\n"
        "    version='1.1'\n"
        "    width='200'\n"
        "    height='200'>\n"
        "\n"
        "<defs>\n"
        "    <linearGradient id='greenGradient' x1='0%' y1='0%' x2='100%' y2='100%'>\n"
        "        <stop offset='0%' style='stop-color:rgb(125,255,125);stop-opacity:0.7' />\n"
        "        <stop offset='150%' style='stop-color:rgb(15,125,15);stop-opacity:0.7' />\n"
        "    </linearGradient>\n"
        "</defs>\n"
        "\n"
        "<rect x='0' y='0' width='200' height='200' rx='30' style='stroke:white;fill:white;opacity:0.7;'/>\n"
        "STATE_MARKER\n"
        "ARC_LEVEL\n"
        "<text x='100' y='135' text-anchor='middle' font-size='100' font-weight='bolder' fill='black'>PERCENT</text>\n"
        "</svg>");

    static QString levelArcs      = QL1S(
        "<path d='M 100,20 A80,80 0, LARGE_ARC_FLAG, SWEEP_FLAG, END_X,END_Y' style='fill:none; stroke:url(#greenGradient); stroke-width:38;' />\n"
        "<path d='M 100,20 A80,80 0, LARGE_ARC_FLAG, SWEEP_FLAG, END_X,END_Y' style='fill:none; stroke:red; stroke-width:38; opacity:RED_OPACITY' />\n");
    static QString levelCircle    = QL1S("<circle cx='100' cy='100' r='80' style='fill:none; stroke:url(#greenGradient); stroke-width:38;' />");
    static QString filledCircle   = QL1S("<circle cx='35' cy='35' r='35'/>");
    static QString plus           = QL1S("<path d='M 0,35 L70,35 M35,0 L35,70' style='stroke:black; stroke-width:25;'/>");
    static QString minus          = QL1S("<path d='M 130,35 L200,35' style='stroke:black; stroke-width:25;'/>");
    static QString hollowCircle   = QL1S("<circle cx='165' cy='35' r='30' style='fill:none;stroke:black;stroke-width:10'/>");

    QString svg = svg_template;

    if (chargeLevel > 99)
    {
      svg.replace(QL1S("ARC_LEVEL"), levelCircle);
    } else
    {
      svg.replace(QL1S("ARC_LEVEL"), levelArcs);
      double angle;
      QString sweepFlag;
      if (state == Solid::Battery::Discharging)
      {
        angle = M_PI_2 + 2 * M_PI * chargeLevel/100;
        sweepFlag = QL1C('0');
      }
      else
      {
        angle = M_PI_2 - 2 *M_PI * chargeLevel/100;
        sweepFlag = QL1C('1');
      }
      double circle_endpoint_x = 80.0 * cos(angle) + 100;
      double circle_endpoint_y = -80.0 * sin(angle) + 100;

      QString largeArgFlag = chargeLevel > 50 ? QL1S("1") : QL1S("0");

      svg.replace(QL1S("END_X"), QString::number(circle_endpoint_x));
      svg.replace(QL1S("END_Y"), QString::number(circle_endpoint_y));
      svg.replace(QL1S("LARGE_ARC_FLAG"), largeArgFlag);
      svg.replace(QL1S("SWEEP_FLAG"), sweepFlag);
    }
    svg.replace(QL1S("PERCENT"), QString::number(chargeLevel));

    switch (state)
    {
    case Solid::Battery::FullyCharged:
        svg.replace(QL1S("STATE_MARKER"), filledCircle);
        break;
    case Solid::Battery::Charging:
        svg.replace(QL1S("STATE_MARKER"), plus);
        break;
    case Solid::Battery::Discharging:
        svg.replace(QL1S("STATE_MARKER"), minus);
        break;
    default:
        svg.replace(QL1S("STATE_MARKER"), hollowCircle);
    }

    if (state != Solid::Battery::FullyCharged && state != Solid::Battery::Charging &&  chargeLevel < mSettings.getPowerLowLevel() + 30)
    {
        if (chargeLevel <= mSettings.getPowerLowLevel() + 10)
            svg.replace(QL1S("RED_OPACITY"), QL1S("1"));
        else
            svg.replace(QL1S("RED_OPACITY"), QString::number((mSettings.getPowerLowLevel() + 30 - chargeLevel)/20.0));
    }
    else
        svg.replace(QL1S("RED_OPACITY"), QL1S("0"));

    // qDebug() << svg;

    // Paint the svg on a pixmap and create an icon from that.
    QSvgRenderer render(svg.toLatin1());
    QPixmap pixmap(render.defaultSize());
    pixmap.fill(QColor(0,0,0,0));
    QPainter painter(&pixmap);
    render.render(&painter);
    return QIcon(pixmap);
}
