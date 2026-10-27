export LC_ALL=C

# Una IP permite solicitar la redirección incluso cuando el DNS del portal todavía falla.
una_probe='http://1.1.1.1/'

# Comprobar también al arrancar si la sesión ya tiene una red con portal.
{ printf '\n'; nmcli monitor; } | while IFS= read -r _event; do
  connectivity=$(nmcli networking connectivity)
  connection=
  ssid=
  while IFS=: read -r type state uuid; do
    if [[ "$type" == wifi && "$state" == connected ]]; then
      connection=$uuid
      ssid=$(nmcli --get-values 802-11-wireless.ssid connection show uuid "$uuid")
      break
    fi
  done < <(nmcli --terse --escape no --fields TYPE,STATE,CON-UUID device status)

  key=
  url=http://nmcheck.gnome.org/check_network_status.txt
  if [[ "${ssid,,}" == una-wifi-sedes ]]; then
    # El comprobador puede estar permitido antes del login: abrir una vez por conexión.
    key="una:$connection"
    # La red determina el dominio, puerto y parámetros vigentes del login.
    url=$una_probe
  elif [[ "$connectivity" == portal ]]; then
    key="portal:$connection"
  fi
  if [[ -n "$key" && "$key" != "${previous:-}" ]]; then
    printf 'Abriendo portal cautivo para %s\n' "${ssid:-la conexión activa}"
    if ! xdg-open "$url"; then
      printf 'No se pudo abrir el portal cautivo\n' >&2
      key=
    fi
  fi
  previous=$key
done
