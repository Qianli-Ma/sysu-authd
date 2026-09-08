'use strict';
'require form';
'require poll';
'require rpc';
'require tools.widgets as widgets';
'require uci';
'require view';

var callStatus = rpc.declare({ object: 'sysu-authd', method: 'status' });
var callPasswordStatus = rpc.declare({ object: 'sysu-authd', method: 'password_status' });
var callSetPassword = rpc.declare({
	object: 'sysu-authd',
	method: 'set_password',
	params: [ 'password' ]
});
var callStart = rpc.declare({ object: 'sysu-authd', method: 'start' });
var callStop = rpc.declare({ object: 'sysu-authd', method: 'stop' });
var callReconnect = rpc.declare({ object: 'sysu-authd', method: 'reconnect' });

function networkStatusText(status) {
	return '%s / %s / %s'.format(status.device || '-', status.wan_ip || '-', status.gateway || '-');
}

function updateStatusValue(id, value) {
	var node = document.getElementById(id);

	if (node)
		node.textContent = value;
}

return view.extend({
	load: function() {
		return Promise.all([
			L.resolveDefault(callStatus(), {}),
			L.resolveDefault(callPasswordStatus(), { configured: false }),
			uci.load('network')
		]);
	},

	render: function(data) {
		var status = data[0] || {};
		var passwordStatus = data[1] || {};
		var wanDevice = uci.get('network', 'wan', 'device') || uci.get('network', 'wan', 'ifname') || '';
		var m = this.map = new form.Map('sysu-authd', _('SYSU Auth'),
			_('Enter your campus-network account and select the physical WAN device. The remaining defaults work for most SYSU Ruijie networks.'));
		var s = m.section(form.NamedSection, 'main', 'sysu-authd', _('Campus network authentication'));
		s.anonymous = true;
		s.tab('basic', _('Basic settings'));
		s.tab('advanced', _('Advanced settings'));

		var state = s.taboption('basic', form.DummyValue, '_state', _('Current state'));
		state.renderWidget = function() {
			return E('span', { id: 'sysu-authd-state' }, status.state || _('Unavailable'));
		};
		var detail = s.taboption('basic', form.DummyValue, '_detail', _('Network status'));
		detail.renderWidget = function() {
			return E('span', { id: 'sysu-authd-network-status' }, networkStatusText(status));
		};
		var lastError = s.taboption('basic', form.DummyValue, '_last_error', _('Last error'));
		lastError.renderWidget = function() {
			return E('span', { id: 'sysu-authd-last-error' }, status.last_error || '-');
		};

		[ [ 'start', _('Start'), callStart ], [ 'stop', _('Stop'), callStop ], [ 'reconnect', _('Reconnect'), callReconnect ] ].forEach(function(action) {
			var button = s.taboption('basic', form.Button, '_' + action[0], action[1]);
			button.inputstyle = action[0] === 'stop' ? 'remove' : 'apply';
			button.onclick = function() {
				return action[2]().then(function() { window.location.reload(); });
			};
		});

		var enabled = s.taboption('basic', form.Flag, 'enabled', _('Enable automatic authentication'),
			_('Start authentication automatically after the router boots.'));
		enabled.default = '0';
		enabled.rmempty = false;

		var username = s.taboption('basic', form.Value, 'username', _('NetID'),
			_('Your SYSU campus-network account name.'));
		username.rmempty = false;

		var password = s.taboption('basic', form.Value, '_password', _('Password'),
			_('Stored separately in /etc/sysu-authd/password with root-only permissions. Leave blank to keep the current password.'));
		password.password = true;
		password.rmempty = true;
		password.placeholder = passwordStatus.configured ? _('Password is configured') : _('Enter your campus-network password');
		password.cfgvalue = function() { return ''; };
		password.remove = function() {};
		password.validate = function(sectionId, value) {
			if (value && (value.length > 255 || /[\r\n]/.test(value)))
				return _('The password must be a single line of at most 255 characters.');
			return true;
		};
		password.write = function(sectionId, value) {
			return value ? callSetPassword(value) : Promise.resolve();
		};

		var device = s.taboption('basic', widgets.DeviceSelect, 'device', _('WAN physical device'),
			_('Select the device connected to the campus-network wall port. The current OpenWrt WAN device is selected by default.'));
		device.default = wanDevice;
		device.noaliases = true;
		device.nocreate = true;
		device.rmempty = false;

		var interfaceName = s.taboption('advanced', form.Value, 'interface', _('OpenWrt logical interface'),
			_('Used to renew DHCP after authentication. Usually leave this as wan.'));
		interfaceName.default = 'wan';
		interfaceName.rmempty = false;

		var backend = s.taboption('advanced', form.ListValue, 'auth_backend', _('Authentication backend'),
			_('Use Ruijie compatibility mode unless your network uses plain EAPOL.'));
		backend.value('ruijie_compat', 'ruijie_compat');
		backend.value('standard_eapol', 'standard_eapol');
		backend.default = 'ruijie_compat';
		var identity = s.taboption('advanced', form.Value, 'identity_format', _('Identity format'),
			_('%u is replaced with the NetID. Most users should keep the default.'));
		identity.default = '%u';
		var version = s.taboption('advanced', form.ListValue, 'eapol_version', _('EAPOL version'),
			_('Version 1 is the most compatible choice for Ruijie networks.'));
		version.value('1');
		version.value('2');
		version.default = '1';

		return m.render().then(function(node) {
			poll.add(function() {
				return L.resolveDefault(callStatus(), {}).then(function(currentStatus) {
					updateStatusValue('sysu-authd-state', currentStatus.state || _('Unavailable'));
					updateStatusValue('sysu-authd-network-status', networkStatusText(currentStatus));
					updateStatusValue('sysu-authd-last-error', currentStatus.last_error || '-');
				});
			}, 2);

			return node;
		});
	}
});
