meta-mixins-lts - scarthgap/rust
================================

A "mixin" layer for adding current Rust toolchain into the Yocto Project LTS.
At the time Scarthgap was released in May 2022 it included Rust 1.59.0, and
officially Scarthgap supports only that. This thin special-purpose mixin
layer is meant to provide a current Rust toolchain for Scarthgap by
backporting the appropriate recipes from the master branch of
openembedded-core.

Notes
-----

- **Not all Rust version updates carried here have been individually tested!**
  Care has been taken to keep backported changes in the same order as they
  have been applied to oe-core, so the various older Rust versions should
  be buildable, but that is not guaranteed.  It is highly recommended that
  you perform your own tests for correctness when using this layer.
- The newer version of librsvg from master has been backported since
  there is not a straightforward way to update the existing one via
  bbappend.  Backporting a working recipe should be easier to maintain
  than developing an update of the older recipe in scarthgap.
- python3-cryptography has been left alone since the recipe in scarthgap
  still works with the newer toolchain, and updating it seems to have
  more potential impact due to upstream API changes and its known to be
  finicky build.  This may change if sufficient rationale for doing the
  backport becomes apparent.
- While changes to Rust recipe and class files related to oe-selftest
  support are included, and the backported Rust test is included as the
  "rust_mixin", it is not guaranteed that it will always be in a passing
  state.
- The intent is to track any further upgrades that occur in the master
  branch of oe-core, with an expected end of support in April 2028, the
  same as Yocto Project's planned EOL for Scarthgap (per
  https://wiki.yoctoproject.org/wiki/Releases).
- At present, no changes related to the addition of UNPACKDIR for the YP
  styhead release have been backported, as it presently looks like trying to
  accommodate them will be more effort than not.  That may change in the
  future.

Dependencies
------------

This layer depends on:

- URI: git://github.com/openembedded/openembedded-core.git  
  layers: meta  
  branch: scarthgap

Contributing
------------

The yocto-patches mailinglist (yocto-patches@lists.yoctoproject.org) is used
for questions, comments and patch review. It is subscriber only, so please
register before posting.

Send pull requests to yocto-patches@lists.yoctoproject.org with
'[meta-lts-mixins][scarthgap/rust]' in the subject.

When sending single patches, please use something like:
git send-email -M -1 --to=yocto-patches@lists.yoctoproject.org --subject-prefix='meta-lts-mixins][scarthgap/rust][PATCH'

Note that changes that are not direct backports from oe-core master branch, or
are not fixes for a demonstrated build breakage against Scarthgap are unlikely
to be accepted.  Functional changes should be submitted and merged to oe-core
master branch first.

See the following section for the requirements for submitting patches
containing bbappends for recipes outside of oe-core using dynamic layers.

Dynamic Layer Contribution Requirements
---------------------------------------

There have been community requests to support bbappends for recipes outside
of oe-core via the use of the dynamic layers scheme.  Submissions will be
considered based on the following criteria:

- Submissions require the submitter committing to documentation of
  themselves as maintainer of the associated bbappends/recipes in the
  "Maintainers" section at the end of this README file.
- Testing of recipes supported by dynamic layers bbappends is up to their
  individual maintainer(s), and they will have any complaints about breakage
  directed their way.
- Dynamic layers will initially only be considered for layers from the
  meta-openembedded repository.
- Non-backport changes to the Rust components from oe-core will by default
  not be accepted to support a dynamic layer bbappend if they can be carried
  in a bbappend in the dynamic layer.  Such changes to the base mixin are
  too likely to increase backport effort over time.
- Straight backports of newer version recipes will generally be
  considered out of scope.  There is some potential for negotiation if a
  package on a LTS branch becomes impossible to compile with a newer Rust
  version, and patching proves extremely difficult compared to a bump to
  a newer version.  Note that version bumps that require backports of
  other components from oe-core will default to being rejected since even
  in a dynamic layer the potential for side effects on downstream users
  can be very difficult to gauge.  An example of this from oe-core would
  be newer versions of python3-cryptography, which require maturin support
  (and a bunch of associated Python infrastructure changes) unavailable in
  kirkstone.

Maintenance
-----------

Layer maintainers:
Scott Murray <scott.murray@konsulko.com>

Dynamic layer support maintainers:
TBD
