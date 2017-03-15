(function () {
    // angular app
    var app = angular.module("app", ['ngMaterial']);

    // angular app's controller
    app.controller("appController", function ($scope, $http, $mdDialog) {
        // url
        var root = 'http://192.168.4.1'
        var apiGetNetworks = '/api/getnetworks';
        var apiConnect = '/api/connect';
        var apiReboot = '/api/reboot';
        var apiSetip = '/api/setip';
        // this line of code to prevent angularjs trigger preflight (OPTIONS) when send with content-type=application/json
        //$http.defaults.headers.post["Content-Type"] = "text/plain"

        // vars        
        $scope.connectedAP = null;
        // functions        
        $scope.isConnectedNetwork = isConnectedNetwork;

        // get networks
        function refreshNetworks() {
            $http.get(root + apiGetNetworks)
        .then(function (response) {
            var data = response.data;
            console.debug(data);
            var avail = data.available;
            $scope.networks = avail.reduce(function (arrays, v) {
                // if v is in list avail
                var contained = arrays.some(function (v2) {
                    return v2.ssid === v.ssid;
                });

                if (!contained)
                    arrays.push(v);

                return arrays;
            }, []);
            $scope.connectedAP = data.network;
            $scope.$applyAsync();
        }, function (ex) {
            console.error(ex);
        });
        }

        function isConnectedNetwork(nw) {
            return $scope.connectedAP && $scope.connectedAP.ssid === nw.ssid;
        }

        // setup ip 
        function doSetIp(dhcp, ip, subnetmask, gateway) {
            console.debug("Update network config: dhcp=" + dhcp + ", ip=" + ip + ", subnetmask=" + subnetmask + ", gateway=" + gateway);
            $http.post(root + apiSetip,
                {
                    dhcp: dhcp,
                    ip: ip,
                    subnetmask: subnetmask,
                    gateway: gateway,
                })
            .then(function (response) {
                console.debug(response);
            },
            function (ex) {
                console.error(ex);
            });
        }

        // do connect to ssid with password
        function doConnect(ssid, password) {
            console.debug("Try connect to ssid=" + ssid + " password=" + password);
            $http.post(root + apiConnect,
                {
                    ssid: ssid,
                    password: password,
                })
            .then(function (response) {
                console.debug(response);
            },
            function (ex) {
                console.error(ex);
            });
        }

        // reboot
        function doReboot() {
            $http.get(root + apiReboot)
            .then(function (response) {
                console.debug(response);
            },
            function (ex) {
                console.error(ex);
            });
        }

        ///////// events
        $scope.onClick = function (nw) {
            // 
            var connected = isConnectedNetwork(nw);
            //open dialog ap detail
            $mdDialog.show({
                controller: DialogAPDetailController,
                templateUrl: 'dialog-ap-detail-template.html',
                parent: angular.element(document.body),//id (in html)
                clickOutsideToClose: true,
                locals: {
                    ap: connected ? $scope.connectedAP : nw,
                    connected: connected,
                }
            })
            .then(
            function (result) {
                console.debug(result);
                // a connected AP -> ok ->
                if (result.update) {
                    //get data from result.settings: dhcp, ip, subnetmask, gateway
                    doSetIp(result.settings.dhcp, result.settings.ip, result.settings.subnetmask, result.settings.gateway);
                }
                else if (result.connect) {
                    //get data from result.settings: ssid and password
                    doConnect(result.settings.ssid, result.settings.password);
                }
            },
            function () {
                console.debug("Dialog cancel");
            });
        }

        $scope.onRefreshNetworks = function () {
            refreshNetworks();
        }

        $scope.onRebootDevice = function () {
            var confirm = $mdDialog.confirm()
                .title("Reboot device?")
                .textContent("Do you want to reboot?")
                .ok("Reboot")
                .cancel("Cancel");

            $mdDialog.show(confirm)
            .then(function () {
                console.debug("Reboot clicked");
                doReboot();
            },
            function () {
                console.debug("Cancel clicked");
            });
        }

        //Dialog AP detail
        function DialogAPDetailController($scope, $mdDialog, ap, connected) {
            $scope.ap = ap;
            $scope.settings = {
                ssid: ap.ssid,
                password: "",
                // belows only if a connected ap
                dhcp: ap.dhcp,
                ip: ap.ip,
                subnetmask: ap.subnetmask,
                gateway: ap.gateway,
            };
            $scope.isConnected = connected;

            $scope.ipSettingChanged = function () {
                return $scope.settings.dhcp != $scope.ap.dhcp
                    || $scope.settings.ip != $scope.ap.ip
                    || $scope.settings.subnetmask != $scope.ap.subnetmask
                    || $scope.settings.gateway != $scope.ap.gateway;
            }

            $scope.onUpdate = function () {
                var changed = $scope.ipSettingChanged();
                // settings change?
                if (changed)
                    $mdDialog.hide(
                        {
                            update: true,
                            settings: $scope.settings
                        });// 
                else $mdDialog.hide({
                    update: false
                });
            };
            $scope.onConnect = function () {
                $mdDialog.hide(
                    {
                        connect: true,
                        settings: $scope.settings
                    });// raise ok
            }
            $scope.onCancel = function () {
                $mdDialog.cancel(); // raise cancel
            };
        }


        ////// call when startup
        refreshNetworks();
    });
})();

///// old Jquery
$(document).ready(function () {
    //App.GetNetworks(false);
    //App.Events.ReloadNetworks();
});

var App = {
    GetNetworksURL: '/ajax/get-networks',
    ConnectURL: '/ajax/connect',
    Connect: null,
    CurrentNetwork: null,
    GetNetworks: function (reconect) {
        (function worker() {
            $('#floatingCirclesG').show();
            var ajax = $.ajax({
                url: App.GetNetworksURL,
                type: 'POST',
                data: {
                    'reconnect': reconect
                },
                success: function (data) {
                    App.SetNetworks(data.available, data.network);
                    App.ShowConnectToNetwork();
                    App.ConnectToNetwork();
                    if (data.network) {
                        App.Utilities.ShowCurrentNetwork($('div[data-name="' + data.network + '"]').parent());
                    }
                }
            })
            .done(function () {
                $('#floatingCirclesG').hide();
            })
            .fail(function () {
                setTimeout(worker, 5000);
            });
            App.Utilities.AbortConnection(ajax);
        })();
    },
    ConnectToNetwork: function () {
        $(".connect-btn").unbind('click');
        $('.connect-btn').on('click', function () {
            var that = this;
            var item = $(that).parent().parent().parent();
            var password = item.find('input[type=password]').val();
            var network = item.find('h4').text();
            App.Utilities.ShowLoader(that);
            if (App.Connect) {
                clearInterval(App.Connect);
                App.Utilities.HideLoader();
            }
            App.Utilities.ShowLoader(that);
            (function worker() {
                $('.error').empty();
                var ajax = $.ajax({
                    url: App.ConnectURL,
                    type: 'POST',
                    data: {
                        'network': network,
                        'password': password
                    },
                    success: function (data) {
                        if (data.connected == true) {
                            $(that).parent().parent().empty();
                            App.Utilities.HideLoader();
                            App.Utilities.ShowCurrentNetwork(item);
                        }
                        if (data.error) {
                            $('.error').append('<div class="alert alert-danger" role="alert">' +
                                    '<span class="glyphicon glyphicon-exclamation-sign" aria-hidden="true"></span>' +
                                    '<span>Error:</span> ' + data.error +
                                    '</div>');
                            App.Utilities.HideLoader();
                        } else if (data.connected == false) {
                            if (data.status == true) {
                                network = null;
                                password = null;
                                App.Connect = setTimeout(worker, 50);
                            } else {
                                App.Utilities.HideLoader();
                            }
                        }
                    },
                    error: function () {
                        setTimeout(worker, 5000);
                    }
                })
                App.Utilities.AbortConnection(ajax);
            })()
        })
    },
    SetNetworks: function (data, network) {
        var ids = App.Utilities.GetNetworksId();
        $.each(data, function (key, val) {
            if (ids.indexOf(val.id) != -1) {
                App.Utilities.UpdateNetworkValues($('.network[data-id=' + val.id + ']'), val);
                ids.splice(ids.indexOf(val.id), 1);
            } else {
                App.AddNetwork(val, network);
            }
        });
        $.each(ids, function (key, val) {
            App.Utilities.RemoveNetwork($('.network[data-id=' + val + ']'));
        });
    },
    ShowConnectToNetwork: function () {
        $(".network").unbind('click');
        $('.network').on('click', function (e) {
            $('.connect').each(function () {
                $(this).hide();
            })
            $(this).find('.connect').show();
        })
    },
    AddNetwork: function (val, network) {
        var connect = '';
        if (val.title != network) {
            var input = '';
            if (val.encryption != 'OPEN') {
                input += '<label>Password: </label><input class="form-control" style="width: 200px" type="password">'
            }

            connect += input +
                    '<div class="connect-btn-wrapper"><button class="btn btn-success connect-btn">Connect</button></div>';
        }
        $('.networks').append(
                '<div class="list-group-item network" href="#" data-id="' + val.id + '">' +
                        '<p class="list-group-item-text">' +
                            '<div>' +
                            '<div class="wifi ' + App.Utilities.GetNetworkSignalClass(val.signal) + '"></div>' +
                            '<div class="network-info">' +
                            '<h4 class="list-group-item-heading">' + val.title + '</h4>' +
                            '<div><span class="encryption"> ' + val.encryption + '</span></div>' +
                            '</div></div>' +
                        '</p>' +
                        '<div class="form-group connect"  style="display: none" data-name="' + val.title + '">' + connect +
                        '</div>' +
                        '</div>'
        );

        return true;
    },
    Events: {
        ReloadNetworks: function () {
            $('.reload').on('click', function () { App.GetNetworks(true) })
        }
    },
    Utilities: {
        AbortConnection: function (ajax) {
            if (ajax.readyState != 4) {
                setTimeout(function () { ajax.abort() }, 6000);
            }
        },
        RemoveNetwork: function (network) {
            $(network).remove();
        },
        GetNetworkSignalClass: function (signal) {
            if (signal >= -100 && signal <= -80) {
                return 'wifi-1';
            } else if (signal > -80 && signal <= -65) {
                return 'wifi-2';
            } else if (signal > -65 && signal < -50) {
                return 'wifi-3';
            } else {
                return 'wifi-4';
            }
        },
        UpdateNetworkValues: function (network, data) {
            $(network).find('h4').text(' ' + data.title);
            $(network).find('.encryption').text(' ' + data.encryption);
            $(network).find('.wifi').removeClass().addClass('wifi').addClass(App.Utilities.GetNetworkSignalClass(data.signal));
        },
        ShowCurrentNetwork: function (obj) {
            console.log(obj);
            if (App.CurrentNetwork) {
                $(App.CurrentNetwork).css('background', '');
            }
            App.CurrentNetwork = obj;
            $(App.CurrentNetwork).css('background', '#5BC0DE');
        },
        GetNetworksId: function () {
            var ids = [];
            $('.network').each(function () {
                ids.push(parseInt($(this).attr('data-id')));
            })
            return ids;
        },
        ShowLoader: function (obj) {
            var loader = $('#circularG');
            loader.show();
            loader.css('margin-left', '100px');
            $(obj).parent().append(loader);
        },
        HideLoader: function () {
            $('#circularG').hide();
        }
    }
}