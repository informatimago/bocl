#!/bin/sh
# ci/freebsd-build.sh — build + test bocl on FreeBSD inside a QEMU VM, from a
# Linux CI runner. The BSDs cannot run in a container (no shared kernel), so we
# boot a FreeBSD cloud image under QEMU (TCG; gitlab.com shared runners have no
# KVM), inject an SSH key + build deps via cloud-init (NoCloud), then run the
# normal ci/build.sh over SSH with MAKE=gmake CC=clang.
#
# Expects to run from the repo root on a Debian/Ubuntu runner. Tunables:
#   FREEBSD_VERSION   default 14.1
#   VM_MEM / VM_CPUS  default 2048 / 2
set -eu

VM_MEM="${VM_MEM:-2048}"
VM_CPUS="${VM_CPUS:-2}"
SSH_PORT=2222
WORK="$(pwd)"
VMI="https://download.freebsd.org/releases/VM-IMAGES"

echo "== installing host QEMU tooling =="
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y --no-install-recommends \
    qemu-system-x86 qemu-utils cloud-image-utils openssh-client \
    xz-utils curl ca-certificates netcat-openbsd >/dev/null

# FreeBSD removes superseded point releases, so discover the latest 14.x rather
# than hard-coding it (override with FREEBSD_VERSION=14.3 etc.).
if [ -z "${FREEBSD_VERSION:-}" ]; then
    FREEBSD_VERSION="$(curl -fsSL "$VMI/" \
        | grep -oE '14\.[0-9]+-RELEASE' | sed 's/-RELEASE//' | sort -V | tail -1)"
    : "${FREEBSD_VERSION:=14.3}"
fi
echo "== using FreeBSD ${FREEBSD_VERSION} =="
IMG_BASE="FreeBSD-${FREEBSD_VERSION}-RELEASE-amd64-BASIC-CLOUDINIT-ufs.qcow2"
IMG_URL="${VMI}/${FREEBSD_VERSION}-RELEASE/amd64/Latest/${IMG_BASE}.xz"

echo "== fetching FreeBSD ${FREEBSD_VERSION} cloud image =="
curl -fsSL "$IMG_URL" -o "${IMG_BASE}.xz"
unxz -f "${IMG_BASE}.xz"
qemu-img resize "$IMG_BASE" 10G

echo "== preparing cloud-init seed (ssh key + build deps) =="
ssh-keygen -t ed25519 -N "" -f id_ci >/dev/null
PUBKEY="$(cat id_ci.pub)"
# Create an explicit `ci` user with our key (the image's default user name and
# root-SSH policy vary; this avoids depending on either). cloud-init runs as
# root, so `packages:` installs gmake/cunit system-wide; the build needs no
# privileges. ci-ready is touched LAST so the wait loop also waits for deps.
cat > user-data <<EOF
#cloud-config
users:
  - name: ci
    shell: /bin/sh
    lock_passwd: true
    ssh_authorized_keys:
      - ${PUBKEY}
package_update: true
packages:
  - gmake
  - cunit
runcmd:
  - [ sh, -c, "touch /home/ci/ci-ready && chown ci /home/ci/ci-ready" ]
EOF
printf 'instance-id: bocl-ci\nlocal-hostname: bocl-ci\n' > meta-data
cloud-localds seed.img user-data meta-data

echo "== booting FreeBSD VM (TCG) =="
qemu-system-x86_64 \
    -machine q35 -m "$VM_MEM" -smp "$VM_CPUS" -accel tcg \
    -drive file="$IMG_BASE",if=virtio,format=qcow2 \
    -drive file=seed.img,if=virtio,format=raw \
    -netdev user,id=n0,hostfwd=tcp::${SSH_PORT}-:22 \
    -device virtio-net,netdev=n0 \
    -nographic -serial file:console.log -monitor none &
QEMU_PID=$!
trap 'kill $QEMU_PID 2>/dev/null || true' EXIT

SSH="ssh -i id_ci -p $SSH_PORT -o StrictHostKeyChecking=no \
        -o UserKnownHostsFile=/dev/null -o ConnectTimeout=8 ci@127.0.0.1"

echo "== waiting for VM + cloud-init deps (TCG boot + pkg install is slow) =="
ready=""
i=0
while [ $i -lt 150 ]; do
    if $SSH 'test -f ~/ci-ready' 2>/dev/null; then ready=1; break; fi
    i=$((i+1)); sleep 10
done
if [ -z "$ready" ]; then
    echo "!! FreeBSD VM did not become ready; serial console tail:" >&2
    tail -60 console.log 2>/dev/null || true
    exit 1
fi
echo "== VM up: $($SSH 'uname -a') =="
$SSH 'gmake --version | head -1; cc --version | head -1; pkg info cunit | head -1' || true

echo "== copying repo into VM =="
$SSH 'mkdir -p ~/bocl'
tar --exclude=.git --exclude='*.qcow2' --exclude='*.xz' --exclude='seed.img' -cf - . \
    | $SSH 'tar -xf - -C ~/bocl'

echo "== building + testing on FreeBSD (gmake, clang) =="
$SSH 'cd ~/bocl && MAKE=gmake CC=clang sh ci/build.sh'
echo "== FreeBSD build OK =="
