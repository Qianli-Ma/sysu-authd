'use strict';
'require form';
'require rpc';
'require view';

var callStatus = rpc.declare({ object: 'sysu-authd', method: 'status' });
var callStart = rpc.declare({ object: 'sysu-authd', method: 'start' });
var callStop = rpc.declare({ object: 'sysu-authd', method: 'stop' });
var callReconnect = rpc.declare({ object: 'sysu-authd', method: 'reconnect' });

return view.extend({
	load: function() {
		return L.resolveDefault(callStatus(), {});
	},

	render: function(data) {
		var status = data || {};
		var m = this.map = new form.Map('sysu-authd', _('SYSU Auth'),
			_('Configure and monitor campus-network 802.1X authentication. Store the password separately in /etc/sysu-authd/password with mode 0600.'));
		var s = m.section(form.NamedSection, 'main', 'sysu-authd', _('Service'));
		s.anonymous = true;

		var state = s.option(form.DummyValue, '_state', _('Current state'));
		state.cfgvalue = function() { return status.state || _('Unavailable'); };
		var detail = s.option(form.DummyValue, '_detail', _('Network status'));
		detail.cfgvalue = function() {
			return '%s / %s / %s'.format(status.device || '-', status.wan_ip || '-', status.gateway || '-');
		};
		var lastError = s.option(form.DummyValue, '_last_error', _('Last error'));
		lastError.cfgvalue = function() { return status.last_error || '-'; };

		[ [ 'start', _('Start'), callStart ], [ 'stop', _('Stop'), callStop ], [ 'reconnect', _('Reconnect'), callReconnect ] ].forEach(function(action) {
			var button = s.option(form.Button, '_' + action[0], action[1]);
			button.inputstyle = action[0] === 'stop' ? 'remove' : 'apply';
			button.onclick = function() {
				return action[2]().then(function() { window.location.reload(); });
			};
		});

		var enabled = s.option(form.Flag, 'enabled', _('Enable service'));
		enabled.rmempty = false;
		s.option(form.Value, 'interface', _('Logical interface')).default = 'wan';
		s.option(form.Value, 'device', _('Physical device')).placeholder = 'eth0';
		var username = s.option(form.Value, 'username', _('NetID'));
		username.rmempty = false;
		s.option(form.Value, 'password_file', _('Password file')).default = '/etc/sysu-authd/password';

		var profile = s.option(form.ListValue, 'profile', _('Profile'));
		profile.value('sysu_ruijie', 'sysu_ruijie');
		var backend = s.option(form.ListValue, 'auth_backend', _('Authentication backend'));
		backend.value('ruijie_compat', 'ruijie_compat');
		backend.value('standard_eapol', 'standard_eapol');
		s.option(form.Value, 'identity_format', _('Identity format')).default = '%u';
		var version = s.option(form.ListValue, 'eapol_version', _('EAPOL version'));
		version.value('1');
		version.value('2');

		[ [ 'dhcp_after_success', _('Renew DHCP after success') ],
		  [ 'healthcheck_enable', _('Enable health checks') ],
		  [ 'logoff_on_stop', _('Send logoff when stopping') ],
		  [ 'reauth_enable', _('Allow reauthentication') ] ].forEach(function(item) {
			s.option(form.Flag, item[0], item[1]).default = '1';
		});

		return m.render();
	}
});
