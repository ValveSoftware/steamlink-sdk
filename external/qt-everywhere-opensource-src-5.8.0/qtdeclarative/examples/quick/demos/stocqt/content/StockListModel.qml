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

import QtQuick 2.0

ListModel {
    id: stocks

    // pre-fetch data for all entries
    Component.onCompleted: {
        for (var idx = 0; idx < count; ++idx) {
            getCloseValue(idx)
        }
    }

    function requestUrl(stockId) {
        var endDate = new Date(""); // today
        var startDate = new Date()
        startDate.setDate(startDate.getDate() - 5);

        var request = "http://ichart.finance.yahoo.com/table.csv?";
        request += "s=" + stockId;
        request += "&g=d";
        request += "&a=" + startDate.getMonth();
        request += "&b=" + startDate.getDate();
        request += "&c=" + startDate.getFullYear();
        request += "&d=" + endDate.getMonth();
        request += "&e=" + endDate.getDate();
        request += "&f=" + endDate.getFullYear();
        request += "&g=d";
        request += "&ignore=.csv";
        return request;
    }

    function getCloseValue(index) {
        var req = requestUrl(get(index).stockId);

        if (!req)
            return;

        var xhr = new XMLHttpRequest;

        xhr.open("GET", req, true);

        xhr.onreadystatechange = function() {
            if (xhr.readyState === XMLHttpRequest.LOADING || xhr.readyState === XMLHttpRequest.DONE) {
                var records = xhr.responseText.split('\n');
                if (records.length > 0 && xhr.status == 200) {
                    var r = records[1].split(',');
                    var today = parseFloat(r[4]);
                    setProperty(index, "value", today.toFixed(2));

                    r = records[2].split(',');
                    var yesterday = parseFloat(r[4]);
                    var change = today - yesterday;
                    if (change >= 0.0)
                        setProperty(index, "change", "+" + change.toFixed(2));
                    else
                        setProperty(index, "change", change.toFixed(2));

                    var changePercentage = (change / yesterday) * 100.0;
                    if (changePercentage >= 0.0)
                        setProperty(index, "changePercentage", "+" + changePercentage.toFixed(2) + "%");
                    else
                        setProperty(index, "changePercentage", changePercentage.toFixed(2) + "%");
                } else {
                    var unknown = "n/a";
                    set(index, {"value": unknown, "change": unknown, "changePercentage": unknown});
                }
            }
        }
        xhr.send()
    }
    // Uncomment to test invalid entries
    // ListElement {name: "The Qt Company"; stockId: "TQTC"; value: "999.0"; change: "0.0"; changePercentage: "0.0"}

    // Data from http://www.nasdaq.com/quotes/nasdaq-100-stocks.aspx
    ListElement {name: "Activision Blizzard Inc."; stockId: "ATVI"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Adobe Systems Inc."; stockId: "ADBE"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Akamai Technologies Inc."; stockId: "AKAM"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Alexion Pharmaceuticals Inc."; stockId: "ALXN"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Alphabet Inc."; stockId: "GOOG"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Alphabet Inc."; stockId: "GOOGL"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Amazon.com Inc."; stockId: "AMZN"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "American Airlines Group Inc."; stockId: "AAL"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Amgen Inc."; stockId: "AMGN"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Analog Devices Inc."; stockId: "ADI"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Apple Inc."; stockId: "AAPL"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Applied Materials Inc."; stockId: "AMAT"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Autodesk Inc."; stockId: "ADSK"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Automatic Data Processing Inc."; stockId: "ADP"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Baidu Inc."; stockId: "BIDU"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Bed Bath & Beyond Inc."; stockId: "BBBY"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Biogen Inc."; stockId: "BIIB"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "BioMarin Pharmaceutical Inc."; stockId: "BMRN"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Broadcom Limited"; stockId: "AVGO"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "CA Inc."; stockId: "CA"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Celgene Corp."; stockId: "CELG"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Cerner Corp."; stockId: "CERN"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Charter Communications Inc."; stockId: "CHTR"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Check Point Software Technologies Ltd."; stockId: "CHKP"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Cisco Systems Inc."; stockId: "CSCO"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Citrix Systems Inc."; stockId: "CTXS"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Cognizant Technology Solutions Corp."; stockId: "CTSH"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Comcast Corp."; stockId: "CMCSA"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Costco Wholesale Corp."; stockId: "COST"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Ctrip.com International Ltd."; stockId: "CTRP"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Discovery Communications Inc."; stockId: "DISCA"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Discovery Communications Inc."; stockId: "DISCK"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "DISH Network Corp."; stockId: "DISH"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Dollar Tree Inc."; stockId: "DLTR"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "eBay Inc."; stockId: "EBAY"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Electronic Arts Inc."; stockId: "EA"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Endo International Plc"; stockId: "ENDP"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Intel Corp."; stockId: "INTC"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Intuit Inc."; stockId: "INTU"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Intuitive Surgical Inc."; stockId: "ISRG"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "JD.com Inc."; stockId: "JD"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "KLA-Tencor Corp."; stockId: "KLAC"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Lam Research Corp."; stockId: "LRCX"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Liberty Global Plc"; stockId: "LBTYA"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Liberty Global Plc"; stockId: "LBTYK"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Liberty Interactive Corp."; stockId: "LVNTA"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Liberty Interactive Corp."; stockId: "QVCA"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Liberty Media Corp."; stockId: "LMCA"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Liberty Media Corp."; stockId: "LMCK"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Linear Technology Corp."; stockId: "LLTC"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Marriott International"; stockId: "MAR"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Mattel Inc."; stockId: "MAT"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Maxim Integrated Products Inc."; stockId: "MXIM"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Micron Technology Inc."; stockId: "MU"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Microsoft Corp."; stockId: "MSFT"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Mondelez International Inc."; stockId: "MDLZ"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Monster Beverage Corp."; stockId: "MNST"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Mylan N.V."; stockId: "MYL"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "NetApp Inc."; stockId: "NTAP"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Netflix Inc."; stockId: "NFLX"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Norwegian Cruise Line Holdings Ltd."; stockId: "NCLH"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "NVIDIA Corp."; stockId: "NVDA"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "NXP Semiconductors N.V."; stockId: "NXPI"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "O'Reilly Automotive Inc."; stockId: "ORLY"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "PACCAR Inc."; stockId: "PCAR"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Paychex Inc."; stockId: "PAYX"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "PayPal Holdings Inc."; stockId: "PYPL"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "QUALCOMM Inc."; stockId: "QCOM"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Regeneron Pharmaceuticals Inc."; stockId: "REGN"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Ross Stores Inc."; stockId: "ROST"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "SanDisk Corp."; stockId: "SNDK"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "SBA Communications Corp."; stockId: "SBAC"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Seagate Technology PLC"; stockId: "STX"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Sirius XM Holdings Inc."; stockId: "SIRI"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Skyworks Solutions Inc."; stockId: "SWKS"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Starbucks Corp."; stockId: "SBUX"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Stericycle Inc."; stockId: "SRCL"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Symantec Corp."; stockId: "SYMC"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "T-Mobile US Inc."; stockId: "TMUS"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Tesla Motors Inc."; stockId: "TSLA"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Texas Instruments Inc."; stockId: "TXN"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "The Kraft Heinz Company"; stockId: "KHC"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "The Priceline Group Inc."; stockId: "PCLN"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Tractor Supply Company"; stockId: "TSCO"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "TripAdvisor Inc."; stockId: "TRIP"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Twenty-First Century Fox Inc."; stockId: "FOX"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Twenty-First Century Fox Inc."; stockId: "FOXA"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Ulta Salon Cosmetics & Fragrance Inc."; stockId: "ULTA"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Verisk Analytics Inc."; stockId: "VRSK"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Vertex Pharmaceuticals Inc."; stockId: "VRTX"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Viacom Inc."; stockId: "VIAB"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Vodafone Group Plc"; stockId: "VOD"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Walgreens Boots Alliance Inc."; stockId: "WBA"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Western Digital Corp."; stockId: "WDC"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Whole Foods Market Inc."; stockId: "WFM"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Xilinx Inc."; stockId: "XLNX"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
    ListElement {name: "Yahoo! Inc."; stockId: "YHOO"; value: "0.0"; change: "0.0"; changePercentage: "0.0"}
}
