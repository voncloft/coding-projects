echo "step:Getting files needed for build and preparing them"
mkdir -v $LFS/sources
chmod -v a+wt $LFS/sources

LFS_PACKAGES_VERSION="${LFS_PACKAGES_VERSION:-13.0}"
LFS_PACKAGES_BASE_URL="https://ftp.osuosl.org/pub/lfs/lfs-packages"
LFS_PACKAGES_ARCHIVE="lfs-packages-${LFS_PACKAGES_VERSION}.tar"

wget -c "${LFS_PACKAGES_BASE_URL}/${LFS_PACKAGES_ARCHIVE}" --directory-prefix="$LFS/sources"
tar -xvf "$LFS/sources/${LFS_PACKAGES_ARCHIVE}" -C "$LFS/sources"
mv $LFS/sources/*/* $LFS/sources
cp ../../tools/autountar.sh $LFS/sources/autountar
cp scripts/universal/versions.sh $LFS/sources
cp -rv ../../scratchpkg_repo $LFS/scratchpkg_repo_instructions
chmod +x $LFS/sources/autountar

git clone https://github.com/voncloft/scratchpkg.git
mv scratchpkg $LFS/sources
rm $LFS/sources/scratchpkg/scratchpkg.repo
echo "#lfs" >> $LFS/sources/scratchpkg/scratchpkg.repo
echo "/scratchpkg_repo_instructions" >> $LFS/sources/scratchpkg/scratchpkg.repo
echo "SOURCE_DIR=/sources" >> $LFS/sources/scratchpkg/scratchpkg.conf
tar cJf $LFS/sources/scratchpkg.tar.xz $LFS/sources/scratchpkg

rm -rfv $LFS/sources/scratchpkg

chown root:root $LFS/sources/*
