#!/usr/bin/env python

# Copyright (c) 2013 Intel Corporation. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import optparse
import os
import re
import shutil
import sys
from xml.dom import minidom

def ReplaceNodeValue(doc, node, name, value):
  item = doc.getElementsByTagName(node)[0]
  item.attributes[name].value = value


def AddAttribute(doc, node, name, value):
  item = doc.getElementsByTagName(node)[0]
  item.setAttribute(name, value)


def AddThemeStyle(doc, node, name, value):
  item = doc.getElementsByTagName(node)[0]
  src_str = item.attributes[name].value
  src_str = src_str + '.' + value
  item.attributes[name].value = src_str


def RemoveThemeStyle(doc, node, name, value):
  item = doc.getElementsByTagName(node)[0]
  dest_str = item.attributes[name].value.replace('.' + value, '')
  item.attributes[name].value = dest_str


def CustomizeXML(options):
  manifest_path = 'app_src/AndroidManifest.xml.template'
  if not os.path.isfile(manifest_path):
    print ('Please make sure AndroidManifest.xml.template'
           ' exists under app_src folder.')
    sys.exit(6)

  xmldoc = minidom.parse(manifest_path)
  ReplaceNodeValue(xmldoc, 'manifest', 'package', options.package)
  app_name = options.package + '.' + options.name + 'Application'
  ReplaceNodeValue(xmldoc, 'application', 'android:name', app_name)
  ReplaceNodeValue(xmldoc, 'application', 'android:label', options.name)
  activity_name = options.package + '.' + options.name + 'Activity'
  ReplaceNodeValue(xmldoc, 'activity', 'android:name', activity_name)
  ReplaceNodeValue(xmldoc, 'activity', 'android:label', options.name)
  if options.fullscreen:
    AddThemeStyle(xmldoc, 'activity', 'android:theme', 'Fullscreen')
  else:
    RemoveThemeStyle(xmldoc, 'activity', 'android:theme', 'Fullscreen')
  if options.icon:
    if not os.path.exists('app_src/res/drawable'):
      os.makedirs('app_src/res/drawable')
    icon_file = os.path.basename(options.icon)
    shutil.copyfile(options.icon, 'app_src/res/drawable/%s' % icon_file)
    icon_name = os.path.splitext(icon_file)[0]
    AddAttribute(xmldoc, 'application',
                 'android:icon', '@drawable/%s' % icon_name)

  file_handle = open('app_src/AndroidManifest.xml', 'wb')
  xmldoc.writexml(file_handle)
  file_handle.close()


def ReplaceString(file_path, src, dest):
  file_handle = open(file_path, 'r')
  src_content = file_handle.read()
  file_handle.close()
  file_handle = open(file_path, 'w')
  dest_content = src_content.replace(src, dest)
  file_handle.write(dest_content)
  file_handle.close()


def CustomizeJava(options):
  src_root = 'app_src/src/org/xwalk/app/template'
  src_activity = '%s/AppTemplateActivity.java.template' % src_root
  src_application = '%s/AppTemplateApplication.java.template' % src_root
  if (not os.path.isfile(src_activity) or
      not os.path.isfile(src_application)):
    print ('Please make sure that the template java files'
           ' of activity and application do exist.')
    sys.exit(7)
  root_path =  'app_src/src/%s/' % options.package.replace('.', '/')
  if not os.path.exists(root_path):
    os.makedirs(root_path)
  dest_activity = root_path + options.name + 'Activity.java'
  dest_application =  root_path + options.name + 'Application.java'
  shutil.copyfile(src_activity, dest_activity)
  shutil.copyfile(src_application, dest_application)
  ReplaceString(dest_activity, 'org.xwalk.app.template', options.package)
  ReplaceString(dest_activity, 'AppTemplate', options.name)
  ReplaceString(dest_application, 'org.xwalk.app.template', options.package)
  ReplaceString(dest_application, 'AppTemplate', options.name)
  if re.search(r'^http(|s)', options.url):
    ReplaceString(dest_activity, 'file:///android_asset/index.html',
                  options.url)
  else:
    ReplaceString(dest_activity, 'index.html', options.url)


def main():
  parser = optparse.OptionParser()
  info = ('The package name. Such as: '
          '--package=com.example.YourPackage')
  parser.add_option('--package', help=info)
  info = ('The apk name. Such as: --name=YourApplicationName')
  parser.add_option('--name', help=info)
  info = ('The path of icon. Such as: --icon=/path/to/your/customized/icon')
  parser.add_option('--icon', help=info)
  info = ('The url of this application. Such as: '
          '--url=index.html or --url=http://www.intel.com')
  parser.add_option('--url', help=info)
  parser.add_option('-f', '--fullscreen', action='store_true',
                    dest='fullscreen', default=False,
                    help='Make application fullscreen.')
  options, _ = parser.parse_args()
  try:
    CustomizeXML(options)
    CustomizeJava(options)
  except SystemExit, ec:
    print 'Exiting with error code: %d' % ec.code
    return ec.code
  return 0


if __name__ == '__main__':
  sys.exit(main())
