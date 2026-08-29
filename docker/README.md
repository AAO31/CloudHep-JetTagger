# Entorno de trabajo: Docker

Esta carpeta documenta como instalar Docker y crear los contenedores usados
para el analisis de CMS Open Data. Se empieza con los contenedores de Python y ROOT (usados para el analisis en formato NanoAOD); mas adelante se agregara aqui tambien la instalacion del contenedor de CMSSW, necesario para etapas del proyecto que trabajen con formatos AOD o MiniAOD.

## Imagenes usadas

- `gitlab-registry.cern.ch/cms-cloud/python-vnc:python3.10.5`
- `gitlab-registry.cern.ch/cms-cloud/root-vnc:latest`

(La imagen de CMSSW se documentara en una actualizacion futura de este
archivo.)

## 1. Instalar Docker

Instrucciones para Ubuntu/Debian y derivados (incluyendo Linux Mint).

```bash
# Eliminar versiones antiguas o en conflicto
sudo apt-get remove docker.io docker-doc docker-compose docker-compose-v2 podman-docker containerd runc

# Actualizar el indice de paquetes
sudo apt-get update

# Instalar certificados y curl
sudo apt-get install ca-certificates curl

# Crear directorio para la llave de Docker
sudo install -m 0755 -d /etc/apt/keyrings

# Descargar la clave GPG oficial de Docker
sudo curl -fsSL https://download.docker.com/linux/ubuntu/gpg -o /etc/apt/keyrings/docker.asc
sudo chmod a+r /etc/apt/keyrings/docker.asc
```

### Nota importante para Linux Mint

Linux Mint no es Ubuntu puro: su `VERSION_CODENAME` (ej. `zena` para Mint 22.3) no es reconocido por el repositorio de Docker, y usar el script automatico de deteccion de codename produce un error de "release not found". La solucion es usar el `UBUNTU_CODENAME` real detras de tu version de Mint (se puede verificar con `cat /etc/os-release`). Por ejemplo, para Mint 22.3 "Zena" (basado en Ubuntu 24.04 "Noble"):

```bash
echo "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.asc] https://download.docker.com/linux/ubuntu noble stable" | sudo tee /etc/apt/sources.list.d/docker.list > /dev/null

sudo apt-get update
sudo apt-get install docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin
```

### Verificar la instalacion

```bash
sudo docker run hello-world
```

Deberia mostrar el mensaje `Hello from Docker!`.

### Usar Docker sin `sudo` (recomendado)

```bash
sudo usermod -aG docker $USER
newgrp docker
docker run hello-world   # deberia funcionar ya sin sudo
```

## 2. Crear el contenedor de Python

```bash
export workpath=$PWD
mkdir cms_open_data_python
chmod -R 777 cms_open_data_python

docker run -it --name Mi_docker_de_Python -P -p 8888:8888 \
  -v ${workpath}/cms_open_data_python:/code \
  gitlab-registry.cern.ch/cms-cloud/python-vnc:python3.10.5
```

Dentro del contenedor, para usar Jupyter Lab:

```bash
jupyter-lab --ip=0.0.0.0 --no-browser
```

Copia el enlace `http://...` que aparece en la consola y abrelo en tu
navegador (si no carga, reemplaza `127.0.0.1` por `localhost`).

## 3. Crear el contenedor de ROOT

```bash
mkdir cms_open_data_root
chmod -R 777 cms_open_data_root

docker run -it --name Mi_docker_de_Root -P -p 5901:5901 -p 6080:6080 \
  -v ${workpath}/cms_open_data_root:/code \
  gitlab-registry.cern.ch/cms-cloud/root-vnc:latest
```

Dentro del contenedor, para gráficos via VNC:

```bash
start_vnc
root
```

Abre en tu navegador: `http://127.0.0.1:6080/vnc.html` (contraseña: `cms.cern`).
Dentro de ROOT, se puede probar con:

```cpp
TBrowser t
```

Para salir de ROOT: `.q`. Para detener VNC: `stop_vnc`.

## 4. Reanudar los contenedores en sesiones futuras

Los contenedores no se eliminan al salir; para retomarlos sin descargar
nada de nuevo:

```bash
docker start -i Mi_docker_de_Python
docker start -i Mi_docker_de_Root
```

## 5. Comandos utiles

| Comando | Que hace |
|---|---|
| `docker ps -a` | Lista todos los contenedores (activos y detenidos) |
| `docker exec -it Mi_docker_de_Root /bin/bash` | Entra a un contenedor que ya esta corriendo, sin detenerlo |
| `docker rm <nombre>` | Elimina un contenedor detenido |
| `docker start -i <nombre>` | Reanuda un contenedor detenido, en modo interactivo |

## Notas

- El volumen montado (`-v host:/code`) es bidireccional: cualquier archivo creado dentro de `/code` en el contenedor aparece automaticamente en la carpeta del host, y viceversa.
- Los nombres de carpeta (`cms_open_data_python`, `cms_open_data_root`) y de contenedor (`Mi_docker_de_Python`, `Mi_docker_de_Root`) son arbitrarios; se documentan aqui con estos nombres de ejemplo para claridad, pero pueden ajustarse a cualquier convencion siempre que se usen de forma consistente entre el `mkdir`, el `chmod` y el flag `-v` del `docker run`.

## 6. Crear el contenedor de CMSSW
