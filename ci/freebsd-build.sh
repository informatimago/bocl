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

FREEBSD_VERSION="${FREEBSD_VERSION:-14.1}"
VM_MEM="${VM_MEM:-2048}"
VM_CPUS="${VM_CPUS:-2}"
SSH_PORT=2222
WORK="$(pwd)"
IMG_BASE="FreeBSD-${FREEBSD_VERSION}-RELEASE-amd64-BASIC-CLOUDINIT-ufs.qcow2"
IMG_URL="https://download.freebsd.org/releases/VM-IMAGES/${FREEBSD_VERSION}-RELEASE/amd64/Latest/${IMG_BASE}.xz"

echo "== installing host QEMU tooling =="
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y --no-install-recommends \
    qemu-system-x86 qemu-utils cloud-image-utils openssh-client \
    xz-utils curl ca-certificates netcat-openbsd >/dev/null

echo "== fetching FreeBSD ${FREEBSD_VERSION} cloud image =="
curl -fsSL "$IMG_URL" -o "${IMG_BASE}.xz"
unxz -f "${IMG_BASE}.xz"
qemu-img resize "$IMG_BASE" 10G

echo "== preparing cloud-init seed (ssh key + build deps) =="
ssh-keygen -t ed25519 -N "" -f id_ci >/dev/null
PUBKEY="$(cat id_ci.pub)"
cat > user-data <<EOF
#cloud-config
ssh_authorized_keys:
  - ${PUBKEY}
package_update: true
packages:
  - gmake
  - cunit
runcmd:
  - [ touch, /tmp/ci-ready ]
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
        -o UserKnownHostsFile=/dev/null -o ConnectTimeout=8 root@127.0.0.1"

echo "== waiting for SSH (cloud-init provisions deps; TCG boot is slow) =="
ready=""
i=0
while [ $i -lt 90 ]; do
    if $SSH 'test -f /tmp/ci-ready' 2>/dev/null; then ready=1; break; fi
    i=$((i+1)); sleep 10
done
if [ -z "$ready" ]; then
    echo "!! FreeBSD VM did not become ready; serial console tail:" >&2
    tail -40 console.log 2>/dev/null || true
    exit 1
fi
echo "== VM up: $($SSH 'uname -a') =="

echo "== copying repo into VM =="
$SSH 'mkdir -p /root/bocl'
tar --exclude=.git --exclude='*.qcow2' --exclude='seed.img' -cf - . \
    | $SSH 'tar -xf - -C /root/bocl'

echo "== building + testing on FreeBSD (gmake, clang) =="
$SSH 'cd /root/bocl && MAKE=gmake CC=clang sh ci/build.sh'
echo "== FreeBSD build OK =="
